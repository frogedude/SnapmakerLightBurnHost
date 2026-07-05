// notification_service.cpp - Email, beep, print completion notifications
#include "host.h"

void PlayCompletionBeep() {
    try {
        UserConfig cfg = HostContext::instance().snapshotConfig();

        if (!cfg.enableSoundAlerts) {
            return;
        }
        MessageBeep(MB_OK);
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] PlayCompletionBeep: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] PlayCompletionBeep: unknown exception" << std::endl;
    }
}

void PlayAlertChime() {
    try {
        UserConfig cfg = HostContext::instance().snapshotConfig();

        if (!cfg.enableSoundAlerts) {
            return;
        }
        MessageBeep(MB_ICONASTERISK);
    }
    catch (const std::exception& e) {
        std::cerr << GetTimeStamp() << "[ERR] PlayAlertChime: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << GetTimeStamp() << "[ERR] PlayAlertChime: unknown exception" << std::endl;
    }
}

// Email callback structure
struct EmailUploadStatus {
    size_t bytes_read;
    const char* payload;
};

static size_t email_payload_source(char* ptr, size_t size, size_t nmemb, void* userp) {
    EmailUploadStatus* upload_ctx = (EmailUploadStatus*)userp;
    size_t room = size * nmemb;

    if (upload_ctx->bytes_read >= strlen(upload_ctx->payload)) {
        return 0;
    }

    size_t len = strlen(upload_ctx->payload) - upload_ctx->bytes_read;
    if (len > room) len = room;

    memcpy(ptr, upload_ctx->payload + upload_ctx->bytes_read, len);
    upload_ctx->bytes_read += len;

    return len;
}

void SendEmailNotification(const std::string& subject, const std::string& body) {
    UserConfig cfg = HostContext::instance().snapshotConfig();

    if (!cfg.enableEmailAlerts) {
        return;
    }

    if (cfg.emailRecipient.empty() || cfg.emailSender.empty() || cfg.emailAppPassword.empty()) {
        std::cerr << GetTimeStamp() << "[ERR] Email not configured. Use /emailconfig to set up." << std::endl;
        return;
    }

    CURL* curl;
    CURLcode res = CURLE_OK;
    struct curl_slist* recipients = NULL;

    curl = curl_easy_init();
    if (!curl) {
        std::cerr << GetTimeStamp() << "[ERR] Failed to initialize CURL for email" << std::endl;
        return;
    }

    std::string payload = "From: Snapmaker Printer <" + cfg.emailSender + ">\r\n";
    payload += "To: " + cfg.emailRecipient + "\r\n";
    payload += "Subject: " + subject + "\r\n";
    payload += "\r\n";
    payload += body + "\r\n";

    std::string url = "smtp://" + cfg.emailSmtpServer + ":" + std::to_string(cfg.emailSmtpPort);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, cfg.emailSender.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, cfg.emailAppPassword.c_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, cfg.emailSender.c_str());

    recipients = curl_slist_append(recipients, cfg.emailRecipient.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    EmailUploadStatus upload_ctx = { 0, payload.c_str() };
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, email_payload_source);
    curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

    std::cout << std::endl;
    std::cout << GetTimeStamp() << "[EMAIL] Sending notification..." << std::flush;

    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cout << "[FAIL]" << std::endl;
        std::cerr << GetTimeStamp() << "[ERR] Email failed: " << curl_easy_strerror(res) << std::endl;
    }
    else {
        std::cout << "[OK]" << std::endl;
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
}

void ConfigureEmailSettings() {
    ClearConsole();
    PrintHeader("EMAIL NOTIFICATION SETTINGS");
    std::cout << "Configure email alerts for print completion, filament runout, and other events.\n\n";
    std::cout << "NOTE: For Gmail, you need an App Password (not your regular password).\n";
    std::cout << "Generate at: Google Account -> Security -> App passwords\n\n";

    UserConfig cfg = HostContext::instance().snapshotConfig();

    std::cout << "Current settings:\n";
    std::cout << "  Email Alerts: " << (cfg.enableEmailAlerts ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "  SMTP Server: " << cfg.emailSmtpServer << ":" << cfg.emailSmtpPort << "\n";
    std::cout << "  Sender email: " << (cfg.emailSender.empty() ? "(not set)" : cfg.emailSender) << "\n";
    std::cout << "  Recipient: " << (cfg.emailRecipient.empty() ? "(not set)" : cfg.emailRecipient) << "\n";
    std::cout << "  App Password: " << (cfg.emailAppPassword.empty() ? "(not set)" : "********") << "\n";
    std::cout << "\n";

    std::cout << "Do you want to modify email settings? (y/N): ";
    int resp = _getch();
    std::cout << std::endl;

    if (resp != 'y' && resp != 'Y') {
        std::cout << "Email settings unchanged.\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return;
    }

    // Toggle enable/disable (global email alerts)
    std::cout << "Enable email alerts? (y/N): ";
    resp = _getch();
    std::cout << std::endl;
    bool newEnable = (resp == 'y' || resp == 'Y');

    // Get SMTP settings
    std::string input;

    std::cout << "SMTP Server [" << cfg.emailSmtpServer << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) cfg.emailSmtpServer = input;

    std::cout << "SMTP Port [" << cfg.emailSmtpPort << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) {
        try {
            cfg.emailSmtpPort = std::stoi(input);
        }
        catch (...) {}
    }

    std::cout << "Sender email address [" << cfg.emailSender << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) cfg.emailSender = input;

    std::cout << "Recipient email address [" << cfg.emailRecipient << "]: ";
    std::getline(std::cin, input);
    if (!input.empty()) cfg.emailRecipient = input;

    std::cout << "App Password (16 chars with spaces, e.g., abcd efgh ijkl mnop): ";
    std::getline(std::cin, input);
    if (!input.empty()) cfg.emailAppPassword = input;

    cfg.enableEmailAlerts = newEnable;

    // Test email if enabled and configured
    if (newEnable && !cfg.emailRecipient.empty() && !cfg.emailSender.empty() && !cfg.emailAppPassword.empty()) {
        std::cout << "\nSending test email... " << std::flush;
        SendEmailNotification("Test from Snapmaker LightBurn Host",
            "This is a test email to verify your email configuration.\n\n"
            "If you received this, your email settings are correct.");
    }

    // Save to config
    HostContext::instance().updateConfig([&cfg](UserConfig& c) {
        c.enableEmailAlerts = cfg.enableEmailAlerts;
        c.emailRecipient = cfg.emailRecipient;
        c.emailSender = cfg.emailSender;
        c.emailAppPassword = cfg.emailAppPassword;
        c.emailSmtpServer = cfg.emailSmtpServer;
        c.emailSmtpPort = cfg.emailSmtpPort;
        return true;
        });
    SaveConfig(HostContext::instance().getConfigPath().string());

    std::cout << GetTimeStamp() << "[OK] Email settings saved to config.json.\n";
    std::cout << "Press any key to continue...\n";
    (void)_getch();
}