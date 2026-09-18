# Security Notes

This project uses Wi-Fi and third-party cloud / smart-home credentials.

## Never commit

- Wi-Fi passwords
- Sinric Pro App Keys
- Sinric Pro App Secrets
- Sinric Pro Device IDs if you consider them sensitive
- private Firebase credentials
- local `.env` or `secrets.h` files

Use `firmware/secrets.example.h` as a template.

If any real credentials have previously been exposed in screenshots, source files, messages, or public commits, rotate them before publishing the repository.
