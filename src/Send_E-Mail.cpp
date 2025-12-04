#include <Arduino.h>
#include "Send_E-Mail.h"
#include "Config.h"
#include <ESP_Mail_Client.h>

// Define retry parameters
#define MAX_ATTEMPTS 3
#define RETRY_DELAY_MS 5000 // 5 seconds

/*************************************
 * Function to send an email       *
 *            start                 *
 ************************************/
void sendEmail(const String &subject, const String &message)
{
    SMTPSession smtp;
    SMTP_Message msg;

    // --- Configure Message ---
    msg.sender.name = "ESP32";
    msg.sender.email = SENDER_EMAIL;
    msg.subject = subject;
    msg.addRecipient("Recipient", RECIPIENT_EMAIL);

    msg.text.content = message;
    msg.text.charSet = "us-ascii";
    msg.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;

    // Set the SMTP server and login credentials
    smtp.callback([](SMTP_Status status)
                  { Serial.println(status.info()); });

    Session_Config session;
    session.server.host_name = SMTP_HOST;
    session.server.port = SMTP_PORT;
    session.login.email = SENDER_EMAIL;
    session.login.password = SENDER_PASSWORD;
    session.login.user_domain = "";

    // --- Retry Loop ---
    for (int i = 1; i <= MAX_ATTEMPTS; i++)
    {
        Serial.printf("\nAttempt %d of %d...\n", i, MAX_ATTEMPTS);

        // 1. Attempt to Connect
        if (!smtp.connect(&session))
        {
            Serial.println("Could not connect to mail server");
            return;
        }
        else
        {
            // 2. Attempt to Send Mail
            if (MailClient.sendMail(&smtp, &msg))
            {
                Serial.println("✅ Email sent successfully!");
                smtp.closeSession();
                return; // **SUCCESS! Exit the function immediately.**
            }
            else
            {
                Serial.print("❌ Error sending Email, ");
                Serial.println(smtp.errorReason().c_str());
            }
            smtp.closeSession();
        }

        // Delay before the next attempt, unless this was the last one
        if (i < MAX_ATTEMPTS)
        {
            Serial.printf("Retrying in %d seconds...\n", RETRY_DELAY_MS / 1000);
            delay(RETRY_DELAY_MS);
        }
    }

    // This code runs only if all attempts failed
    Serial.println("🛑 All attempts failed. Email NOT sent.");
}
/***************************************
 *  Function to send an email          *
 *            end                      *
 ***************************************/