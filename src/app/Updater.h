#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace gnarl {

// Embedded Ed25519 public key (hex). The private key lives only in ~/.gnarl/signing-key and the
// GitHub Actions secret GNARL_SIGNING_KEY.
constexpr const char* kUpdatePublicKey = "10345ed9307eb97725645745c5eccaf048466e3fd364391ff50ca3c0f6301419";
constexpr const char* kManifestUrl = "https://github.com/dgaster7/gnarl/releases/latest/download/manifest.json";

struct VersionNumber {
    int a = 0, b = 0, c = 0;
    static VersionNumber parse(const juce::String& s);
    bool operator<(const VersionNumber& o) const { return a != o.a ? a < o.a : (b != o.b ? b < o.b : c < o.c); }
    juce::String str() const { return juce::String(a) + "." + juce::String(b) + "." + juce::String(c); }
};

class Updater : public juce::ChangeBroadcaster, private juce::Thread {
public:
    enum class Mode { Off, Notify, Auto };
    enum class Status { Idle, Checking, UpToDate, UpdateAvailable, Downloading, Installed, Error, Offline };
    struct Asset { juce::String name, url, sha256; juce::int64 size = 0; };
    struct Manifest {
        juce::String version, minOs, notes; int contentVersion = 0; juce::String pluginVersion;
        juce::Array<Asset> assets;
        const Asset* find(const juce::String& n) const { for (auto& a : assets) if (a.name == n) return &a; return nullptr; }
    };

    Updater();
    ~Updater() override;

    // settings
    Mode mode() const { return settingsMode; }
    void setMode(Mode m);
    // Background check if the last check is older than 24 h (and mode != Off). Never blocks.
    void maybeCheckDaily();
    void checkNow();
    void installUpdate();          // content and/or plugin, whichever is newer
    void rollBack();               // swap the installed plugins with the backup
    bool hasBackup() const;
    juce::String backupVersion() const;

    Status status() const { return currentStatus.load(); }
    juce::String statusText() const;
    juce::String latestVersion() const { const juce::ScopedLock l(lock); return manifest.version; }
    juce::String releaseNotes() const { const juce::ScopedLock l(lock); return manifest.notes; }
    bool pluginUpdateAvailable() const;
    bool contentUpdateAvailable() const;
    int installedContentVersion() const;
    bool restartNeeded() const { return needsRestart.load(); }

    // ---- primitives (public for tests) ----
    static bool verifySignature(const juce::MemoryBlock& message, const juce::String& sigHex, const juce::String& publicKeyHex);
    static juce::String sha256Hex(const juce::File& f);
    static bool parseManifest(const juce::String& json, Manifest& out);
    static juce::File appDir();
    static juce::File userVst3Dir();
    static juce::File userComponentsDir();
    // Synchronous operations used by the worker thread and by tests. Return an empty string on success.
    juce::String doCheck(juce::URL manifestUrl);
    juce::String doInstallContent();
    juce::String doInstallPlugins(const juce::File& vst3Dir, const juce::File& componentsDir);
    static juce::String doRollBack(const juce::File& vst3Dir, const juce::File& componentsDir);
    void setManifestUrl(const juce::String& u) { manifestUrl = u; }
    // Test hook: GNARL_UPDATER_SELFTEST=<result-file> makes a new plugin instance check + install and write the outcome there.
    void runSelfTestIfRequested();

private:
    void run() override;
    enum class Job { None, Check, Install, Rollback };
    std::atomic<Job> pendingJob{Job::None};
    std::atomic<Status> currentStatus{Status::Idle};
    std::atomic<bool> needsRestart{false};
    juce::CriticalSection lock;
    Manifest manifest; bool haveManifest = false;
    juce::String lastError;
    Mode settingsMode = Mode::Notify;
    juce::String manifestUrl{kManifestUrl};
    void loadSettings();
    void saveSettings(juce::int64 lastCheck = -1);
    void setStatus(Status s, const juce::String& err = {});
    bool download(const juce::URL& url, const juce::File& dest, juce::String& err);
};

} // namespace gnarl
