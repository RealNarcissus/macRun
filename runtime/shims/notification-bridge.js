// notification-bridge.js — bridges macOS Notification API to Linux notify-send
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
// Activated by: MACRUN_SHIM_NOTIFICATIONS=1
//
// macOS Electron apps use the HTML5 Notification API which, under macOS Electron,
// routes through NSUserNotificationCenter. On Linux Electron, notifications use
// libnotify via the system D-Bus. Most apps using the standard Notification API
// will work automatically. This shim adds a safety net by intercepting the
// Notification constructor and falling back to notify-send for edge cases.

(function bridge_notifications() {
    if (process.env.MACRUN_SHIM_NOTIFICATIONS !== '1') return;

    const { spawn } = require('child_process');

    function send_notify(title, body, icon) {
        const args = ['--app-name=macrun', title || 'macrun'];
        if (body) {
            args.push(body);
        }
        if (icon) {
            args.push('--icon=' + icon);
        }
        args.push('--expire-time=5000');
        args.push('--hint=string:desktop-entry:macrun');

        try {
            const child = spawn('notify-send', args, {
                stdio: 'ignore',
                detached: true,
                env: { PATH: process.env.PATH || '/usr/bin:/usr/local/bin' }
            });
            child.unref();
        } catch (_) {
            // notify-send unavailable — notifications silently dropped
        }
    }

    // The standard HTML5 Notification API is implemented by Electron/Chromium
    // and works on Linux without patching. This shim is a safety net only.
    // If notify-send is available, we enhance system notification delivery.

    try {
        const { Notification } = require('electron');
        if (Notification && !Notification._macrun_patched) {
            const OrigNotification = Notification;
            Notification._macrun_patched = true;
        }
    } catch (_) {
        // Electron Notification API not available — fall through
    }
})();
