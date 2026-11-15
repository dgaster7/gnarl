#include "Updater.h"
#include <CommonCrypto/CommonDigest.h>
#include <juce_cryptography/juce_cryptography.h>
#include "monocypher-ed25519.h"

namespace gnarl {

namespace {
bool unhex(const juce::String& h, uint8_t* out, size_t n) {
    auto t = h.trim();
    if ((size_t) t.length() != n * 2) return false;
    for (size_t i = 0; i < n; ++i) {
        auto byte = t.substring((int) i * 2, (int) i * 2 + 2);
        if (!byte.containsOnly("0123456789abcdefABCDEF")) return false;
        out[i] = (uint8_t) byte.getHexValue32();
    }
    return true;
}
const char* kBundles[4][2] = {{"VST3", "GNARL.vst3"}, {"VST3", "GNARL FX.vst3"}, {"Components", "GNARL.component"}, {"Components", "GNARL FX.component"}};

int runTool(const juce::StringArray& args, juce::String* output = nullptr) {
    juce::ChildProcess p;
    if (!p.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr)) return -1;
    auto out = p.readAllProcessOutput();
    p.waitForProcessToFinish(60000);
    if (output) *output = out;
    return (int) p.getExitCode();
}
} // namespace

VersionNumber VersionNumber::parse(const juce::String& s) {
    VersionNumber v;
    auto parts = juce::StringArray::fromTokens(s.trimCharactersAtStart("vV"), ".", "");
    if (parts.size() > 0) v.a = parts[0].getIntValue();
    if (parts.size() > 1) v.b = parts[1].getIntValue();
    if (parts.size() > 2) v.c = parts[2].getIntValue();
    return v;
}

Updater::Updater() : juce::Thread("GNARL updater") {
    loadSettings();
    if (auto* u = std::getenv("GNARL_UPDATE_MANIFEST_URL")) manifestUrl = u; // test/staging override
    startThread(juce::Thread::Priority::low);
    runSelfTestIfRequested();
}

Updater::~Updater() { signalThreadShouldExit(); notify(); stopThread(30000); removeAllChangeListeners(); }

juce::File Updater::appDir() { return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support").getChildFile("GNARL"); }
juce::File Updater::userVst3Dir() { return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/VST3"); }
juce::File Updater::userComponentsDir() { return juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Plug-Ins/Components"); }

void Updater::loadSettings() {
    auto v = juce::JSON::parse(appDir().getChildFile("updater.json"));
    auto m = v.getProperty("mode", "notify").toString();
    settingsMode = m == "off" ? Mode::Off : (m == "auto" ? Mode::Auto : Mode::Notify);
}

void Updater::saveSettings(juce::int64 lastCheck) {
    auto f = appDir().getChildFile("updater.json");
    auto v = juce::JSON::parse(f);
    auto* o = v.getDynamicObject();
    juce::DynamicObject::Ptr obj = o ? o : new juce::DynamicObject();
    obj->setProperty("mode", settingsMode == Mode::Off ? "off" : (settingsMode == Mode::Auto ? "auto" : "notify"));
    if (lastCheck >= 0) obj->setProperty("lastCheck", lastCheck);
    f.getParentDirectory().createDirectory();
    f.replaceWithText(juce::JSON::toString(juce::var(obj.get())));
}

void Updater::setMode(Mode m) { settingsMode = m; saveSettings(); sendChangeMessage(); }

void Updater::maybeCheckDaily() {
    if (settingsMode == Mode::Off) return;
    auto v = juce::JSON::parse(appDir().getChildFile("updater.json"));
    juce::int64 last = (juce::int64) v.getProperty("lastCheck", 0);
    if (juce::Time::currentTimeMillis() - last > 24LL * 3600 * 1000) checkNow();
}

void Updater::checkNow() { pendingJob = Job::Check; notify(); }
void Updater::installUpdate() { pendingJob = Job::Install; notify(); }
void Updater::rollBack() { pendingJob = Job::Rollback; notify(); }

void Updater::setStatus(Status s, const juce::String& err) {
    { const juce::ScopedLock l(lock); if (err.isNotEmpty()) lastError = err; }
    currentStatus = s;
    sendChangeMessage(); // thread-safe and asynchronous; never outlives the broadcaster
}

juce::String Updater::statusText() const {
    const juce::ScopedLock l(lock);
    switch (currentStatus.load()) {
        case Status::Idle: return "Not checked yet";
        case Status::Checking: return "Checking for updates...";
        case Status::UpToDate: return "Up to date (" + juce::String(GNARL_VERSION_STRING) + ")";
        case Status::UpdateAvailable: return "Version " + manifest.version + " is available";
        case Status::Downloading: return "Downloading and verifying...";
        case Status::Installed: return needsRestart ? "Installed. Restart your DAW to use the new version." : "Installed. New presets and wavetables are ready.";
        case Status::Offline: return "Offline - will try again later";
        case Status::Error: return "Update failed: " + lastError;
    }
    return {};
}

bool Updater::pluginUpdateAvailable() const {
    const juce::ScopedLock l(lock);
    return haveManifest && VersionNumber::parse(GNARL_VERSION_STRING) < VersionNumber::parse(manifest.pluginVersion.isNotEmpty() ? manifest.pluginVersion : manifest.version);
}
int Updater::installedContentVersion() const {
    auto f = appDir().getChildFile("Content").getChildFile("version.txt");
    int v = f.existsAsFile() ? f.loadFileAsString().getIntValue() : 0;
    return std::max(v, (int) GNARL_CONTENT_VERSION);
}
bool Updater::contentUpdateAvailable() const { const juce::ScopedLock l(lock); return haveManifest && manifest.contentVersion > installedContentVersion(); }

bool Updater::hasBackup() const { return backupVersion().isNotEmpty(); }
juce::String Updater::backupVersion() const {
    auto d = appDir().getChildFile("Backup");
    for (const auto& e : juce::RangedDirectoryIterator(d, false, "*", juce::File::findDirectories)) return e.getFile().getFileName();
    return {};
}

// ---------------- primitives ----------------
bool Updater::verifySignature(const juce::MemoryBlock& message, const juce::String& sigHex, const juce::String& pubHex) {
    uint8_t sig[64], pk[32];
    if (!unhex(sigHex, sig, 64) || !unhex(pubHex, pk, 32)) return false;
    return crypto_ed25519_check(sig, pk, (const uint8_t*) message.getData(), message.getSize()) == 0;
}

juce::String Updater::sha256Hex(const juce::File& f) {
    juce::FileInputStream in(f);
    if (!in.openedOk()) return {};
    CC_SHA256_CTX ctx; CC_SHA256_Init(&ctx);
    juce::HeapBlock<char> buf(1 << 16);
    for (;;) { auto n = in.read(buf.get(), 1 << 16); if (n <= 0) break; CC_SHA256_Update(&ctx, buf.get(), (CC_LONG) n); }
    uint8_t h[CC_SHA256_DIGEST_LENGTH]; CC_SHA256_Final(h, &ctx);
    return juce::String::toHexString(h, sizeof h, 0);
}

bool Updater::parseManifest(const juce::String& json, Manifest& m) {
    auto v = juce::JSON::parse(json);
    if (!v.isObject()) return false;
    m.version = v["version"].toString();
    m.pluginVersion = v["pluginVersion"].toString();
    m.contentVersion = (int) v["contentVersion"];
    m.minOs = v["minimumOS"].toString();
    m.notes = v["releaseNotes"].toString();
    m.assets.clear();
    if (auto* a = v["assets"].getArray())
        for (auto& x : *a) m.assets.add({x["name"].toString(), x["url"].toString(), x["sha256"].toString().toLowerCase(), (juce::int64) x["size"]});
    return m.version.isNotEmpty() && !m.assets.isEmpty();
}

bool Updater::download(const juce::URL& url, const juce::File& dest, juce::String& err) {
    dest.deleteFile();
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs(15000).withNumRedirectsToFollow(10);
    std::unique_ptr<juce::InputStream> in(url.createInputStream(opts));
    if (!in) { err = "offline"; return false; }
    juce::FileOutputStream out(dest);
    if (!out.openedOk()) { err = "cannot write " + dest.getFullPathName(); return false; }
    juce::HeapBlock<char> buf(1 << 16);
    for (;;) {
        if (threadShouldExit()) { err = "cancelled"; return false; }
        auto n = in->read(buf.get(), 1 << 16);
        if (n <= 0) break;
        out.write(buf.get(), (size_t) n);
    }
    out.flush();
    return dest.getSize() > 0;
}

juce::String Updater::doCheck(juce::URL url) {
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gnarl-update");
    tmp.createDirectory();
    auto mf = tmp.getChildFile("manifest.json"), sf = tmp.getChildFile("manifest.json.sig");
    juce::String err;
    if (!download(url, mf, err)) return err;
    juce::URL sigUrl(url.toString(false) + ".sig");
    if (!download(sigUrl, sf, err)) return "signature missing";
    juce::MemoryBlock mb; mf.loadFileAsData(mb);
    if (!verifySignature(mb, sf.loadFileAsString().trim(), kUpdatePublicKey)) return "manifest signature is invalid (update refused)";
    Manifest m;
    if (!parseManifest(mf.loadFileAsString(), m)) return "manifest unreadable";
    { const juce::ScopedLock l(lock); manifest = m; haveManifest = true; }
    saveSettings(juce::Time::currentTimeMillis());
    return {};
}

juce::String Updater::doInstallContent() {
    Manifest m; { const juce::ScopedLock l(lock); if (!haveManifest) return "no manifest"; m = manifest; }
    const Asset* a = m.find("GNARL-content.zip");
    if (!a) return "release has no content package";
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gnarl-update");
    tmp.createDirectory();
    auto zipf = tmp.getChildFile("GNARL-content.zip");
    juce::String err;
    if (!download(juce::URL(a->url), zipf, err)) return err;
    if (sha256Hex(zipf) != a->sha256) return "content package hash mismatch (update refused)";
    auto staging = tmp.getChildFile("content-staging");
    staging.deleteRecursively(); staging.createDirectory();
    juce::ZipFile zip(zipf);
    if (zip.getNumEntries() == 0 || !zip.uncompressTo(staging, true).wasOk()) return "content package could not be unpacked";
    auto src = staging.getChildFile("Content").isDirectory() ? staging.getChildFile("Content") : staging;
    auto dst = appDir().getChildFile("Content");
    auto old = appDir().getChildFile("Content.old");
    old.deleteRecursively();
    if (dst.exists() && !dst.moveFileTo(old)) return "cannot replace content folder";
    if (!src.moveFileTo(dst)) { old.moveFileTo(dst); return "cannot install content"; }
    old.deleteRecursively();
    dst.getChildFile("version.txt").replaceWithText(juce::String(m.contentVersion));
    return {}; // the User bank is in a separate folder and is never touched
}

juce::String Updater::doInstallPlugins(const juce::File& vst3Dir, const juce::File& compDir) {
    Manifest m; { const juce::ScopedLock l(lock); if (!haveManifest) return "no manifest"; m = manifest; }
    const Asset* a = m.find("GNARL-macOS.zip");
    if (!a) return "release has no plugin package";
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("gnarl-update");
    tmp.createDirectory();
    auto zipf = tmp.getChildFile("GNARL-macOS.zip");
    juce::String err;
    if (!download(juce::URL(a->url), zipf, err)) return err;
    if (sha256Hex(zipf) != a->sha256) return "plugin package hash mismatch (update refused)";
    auto staging = tmp.getChildFile("plugin-staging");
    staging.deleteRecursively(); staging.createDirectory();
    // ditto keeps bundle symlinks and permissions intact
    if (runTool({"/usr/bin/ditto", "-x", "-k", zipf.getFullPathName(), staging.getFullPathName()}) != 0) return "plugin package could not be unpacked";
    auto findBundle = [&](const juce::String& name) -> juce::File {
        for (const auto& e : juce::RangedDirectoryIterator(staging, true, name, juce::File::findDirectories)) return e.getFile();
        return {};
    };
    juce::File newB[4];
    for (int i = 0; i < 4; ++i) { newB[i] = findBundle(kBundles[i][1]); if (!newB[i].isDirectory()) return juce::String("package is missing ") + kBundles[i][1]; }
    // backup: keep exactly one previous version
    auto backupRoot = appDir().getChildFile("Backup");
    backupRoot.deleteRecursively();
    auto backup = backupRoot.getChildFile(GNARL_VERSION_STRING);
    juce::File dirs[2] = {vst3Dir, compDir};
    for (auto& d : dirs) d.createDirectory();
    juce::Array<int> moved;
    for (int i = 0; i < 4; ++i) {
        auto cur = (i < 2 ? vst3Dir : compDir).getChildFile(kBundles[i][1]);
        if (cur.exists()) {
            auto b = backup.getChildFile(kBundles[i][0]).getChildFile(kBundles[i][1]);
            b.getParentDirectory().createDirectory();
            if (!cur.moveFileTo(b)) { err = "cannot back up " + cur.getFullPathName(); break; }
            moved.add(i);
        }
    }
    auto restore = [&] {
        for (int i = 0; i < 4; ++i) {
            auto cur = (i < 2 ? vst3Dir : compDir).getChildFile(kBundles[i][1]);
            auto b = backup.getChildFile(kBundles[i][0]).getChildFile(kBundles[i][1]);
            if (b.exists()) { cur.deleteRecursively(); b.moveFileTo(cur); }
        }
    };
    if (err.isNotEmpty()) { restore(); return err; }
    for (int i = 0; i < 4 && err.isEmpty(); ++i) {
        auto dest = (i < 2 ? vst3Dir : compDir).getChildFile(kBundles[i][1]);
        if (!newB[i].moveFileTo(dest)) err = "cannot install " + dest.getFullPathName();
        else {
            runTool({"/usr/bin/xattr", "-dr", "com.apple.quarantine", dest.getFullPathName()});
            juce::String out;
            if (runTool({"/usr/bin/codesign", "--verify", "--deep", "--strict", dest.getFullPathName()}, &out) != 0) err = "signature check failed for " + dest.getFileName() + ": " + out.trim();
        }
    }
    if (err.isNotEmpty()) { restore(); return err; }
    runTool({"/usr/bin/killall", "-9", "AudioComponentRegistrar"}); // refresh the AU cache for the next host launch
    needsRestart = true;
    return {};
}

juce::String Updater::doRollBack(const juce::File& vst3Dir, const juce::File& compDir) {
    auto backupRoot = appDir().getChildFile("Backup");
    juce::File backup;
    for (const auto& e : juce::RangedDirectoryIterator(backupRoot, false, "*", juce::File::findDirectories)) backup = e.getFile();
    if (!backup.isDirectory()) return "no previous version to roll back to";
    // swap: current -> staging, backup -> current, staging -> backup (so you can roll forward again)
    auto staging = backupRoot.getChildFile(".swap");
    staging.deleteRecursively();
    juce::String installedVersion = GNARL_VERSION_STRING;
    for (int i = 0; i < 4; ++i) {
        auto cur = (i < 2 ? vst3Dir : compDir).getChildFile(kBundles[i][1]);
        auto b = backup.getChildFile(kBundles[i][0]).getChildFile(kBundles[i][1]);
        if (!b.exists()) continue;
        auto s = staging.getChildFile(kBundles[i][0]).getChildFile(kBundles[i][1]);
        s.getParentDirectory().createDirectory();
        if (cur.exists() && !cur.moveFileTo(s)) return "cannot move " + cur.getFullPathName();
        if (!b.moveFileTo(cur)) { s.moveFileTo(cur); return "cannot restore " + b.getFullPathName(); }
        runTool({"/usr/bin/xattr", "-dr", "com.apple.quarantine", cur.getFullPathName()});
        // read the version of what we just moved away
        auto plist = s.getChildFile("Contents/Info.plist");
        if (auto xml = juce::parseXML(plist)) {
            if (auto* dict = xml->getChildByName("dict"))
                for (auto* k = dict->getFirstChildElement(); k; k = k->getNextElement())
                    if (k->getAllSubText() == "CFBundleShortVersionString" && k->getNextElement()) installedVersion = k->getNextElement()->getAllSubText();
        }
    }
    auto newBackup = backupRoot.getChildFile(installedVersion);
    backup.deleteRecursively();
    staging.moveFileTo(newBackup);
    runTool({"/usr/bin/killall", "-9", "AudioComponentRegistrar"});
    return {};
}

void Updater::run() {
    while (!threadShouldExit()) {
        wait(-1);
        if (threadShouldExit()) break;
        Job job = pendingJob.exchange(Job::None);
        if (job == Job::Check || job == Job::Install) {
            setStatus(Status::Checking);
            auto err = doCheck(juce::URL(manifestUrl));
            if (err == "offline") { setStatus(Status::Offline); continue; }
            if (err.isNotEmpty()) { setStatus(Status::Error, err); continue; }
            const bool any = pluginUpdateAvailable() || contentUpdateAvailable();
            if (!any) { setStatus(Status::UpToDate); continue; }
            if (job == Job::Check && settingsMode != Mode::Auto) { setStatus(Status::UpdateAvailable); continue; }
            setStatus(Status::Downloading);
            if (contentUpdateAvailable()) { err = doInstallContent(); if (err.isNotEmpty()) { setStatus(err == "offline" ? Status::Offline : Status::Error, err); continue; } }
            if (pluginUpdateAvailable()) { err = doInstallPlugins(userVst3Dir(), userComponentsDir()); if (err.isNotEmpty()) { setStatus(err == "offline" ? Status::Offline : Status::Error, err); continue; } }
            setStatus(Status::Installed);
        } else if (job == Job::Rollback) {
            auto err = doRollBack(userVst3Dir(), userComponentsDir());
            if (err.isNotEmpty()) setStatus(Status::Error, err);
            else { needsRestart = true; setStatus(Status::Installed); }
        }
    }
}

void Updater::runSelfTestIfRequested() {
    auto* path = std::getenv("GNARL_UPDATER_SELFTEST");
    if (!path) return;
    juce::File result(path);
    juce::String log;
    auto err = doCheck(juce::URL(manifestUrl));
    log << "check: " << (err.isEmpty() ? "ok " + manifest.version : err) << "\n";
    if (err.isEmpty()) {
        if (contentUpdateAvailable()) { auto e = doInstallContent(); log << "content: " << (e.isEmpty() ? "installed" : e) << "\n"; }
        else log << "content: up to date\n";
        if (pluginUpdateAvailable()) { auto e = doInstallPlugins(userVst3Dir(), userComponentsDir()); log << "plugins: " << (e.isEmpty() ? "installed" : e) << "\n"; }
        else log << "plugins: up to date\n";
    }
    if (auto* rb = std::getenv("GNARL_UPDATER_SELFTEST_ROLLBACK")) { (void) rb; auto e = doRollBack(userVst3Dir(), userComponentsDir()); log << "rollback: " << (e.isEmpty() ? "ok" : e) << "\n"; }
    result.replaceWithText(log);
}

} // namespace gnarl
