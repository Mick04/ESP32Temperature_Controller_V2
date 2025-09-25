#include <Arduino.h>
#include "Send_E-Mail.h"
#include "Config.h"
#include <ESP_Mail_Client.h>

/*************************************
 * Function to send an email       *
 *            start                 *
 ************************************/
void sendEmail(const String &subject, const String &message)
{
    SMTPSession smtp;
    SMTP_Message msg;

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

    if (!smtp.connect(&session))
    {
        Serial.println("Could not connect to mail server");
        return;
    }

    if (!MailClient.sendMail(&smtp, &msg))
    {
        Serial.print("Error sending Email, ");
        Serial.println(smtp.errorReason().c_str());
    }
    else
    {
        Serial.println("Email sent successfully!");
    }
    smtp.closeSession();
}
/***************************************
 *  Function to send an email          *
 *            end                      *
 ***************************************/