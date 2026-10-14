// ============================================================
// CreateOS 0.0.1
// ============================================================

#include "user_syscall.h"
#include "../../../kernel/header/proc/user_name.h"

typedef struct {
    const char* language;
    const char* keyboard;
    const char* region;
    const char* timezone;
} firstboot_settings_t;

static void firstboot_step(int step, const char* title,
                           const char* description)
{
    sys_clear();
    sys_puts("============================================================\n",
             0x0B);
    sys_puts("  CREATEOS  /  FIRST-BOOT SETUP\n", 0x0F);
    sys_puts("============================================================\n\n",
             0x0B);
    printf("  STEP %d OF 6   ", step);
    for (int i = 0; i < 20; i++)
        sys_puts(i < step * 20 / 6 ? "#" : "-", i < step * 20 / 6
                     ? 0x0B : 0x08);
    sys_puts("\n\n", 0x07);
    printf("  %s\n", title);
    sys_puts("  ----------------------------------------------------------\n",
             0x08);
    printf("  %s\n\n", description);
}

static void firstboot_creation_log(const char* message, int success)
{
    sys_puts(success ? "  [ OK ] " : "  [FAIL] ",
             success ? 0x0A : 0x0C);
    sys_puts(message, success ? 0x0F : 0x0C);
    sys_putchar('\n');
}

/* Read a line from input (visible) */
static void firstboot_readline(char* buf, int max) {
    int len = 0;
    buf[0] = 0;

    while (1) {
        char c = sys_getc();
        if (!c) continue;

        if (c == '\n' || c == '\r') {
            sys_putchar('\n');
            buf[len] = 0;
            return;
        }

        if (c == '\t' || (unsigned char)c == KEY_TAB ||
            (unsigned char)c == KEY_SHIFT_TAB) {
            if (len < max - 1) {
                buf[len++] = ' ';
                buf[len] = 0;
                sys_putchar(' ');
            }
            continue;
        }

        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = 0;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F)
            continue;

        if (len < max - 1) {
            buf[len++] = c;
            buf[len] = 0;
            sys_putchar(c);
        }
    }
}

static int firstboot_choose(const char* prompt, const char* first,
                            const char* second)
{
    char choice[8];
    for (;;) {
        sys_puts(prompt, 0x0F);
        firstboot_readline(choice, sizeof(choice));
        if (strcmp(choice, "1") == 0) return 1;
        if (strcmp(choice, "2") == 0) return 2;
        sys_puts("Enter 1 or 2.\n\n", 0x0C);
        printf("  1) %s\n  2) %s\n", first, second);
    }
}

static void firstboot_select_settings(firstboot_settings_t* settings)
{
    firstboot_step(1, "Application language",
                   "Choose the default language for applications.");
    sys_puts("  1) English\n  2) Francais\n\n", 0x0E);
    settings->language =
        firstboot_choose("Language [1-2]: ", "English", "Francais") == 1
            ? "en_US" : "fr_FR";

    firstboot_step(2, "Keyboard layout",
                   "The selected layout becomes active immediately.");
    sys_puts("  1) US\n  2) French AZERTY\n\n", 0x0E);
    settings->keyboard =
        firstboot_choose("Keyboard [1-2]: ", "US", "French AZERTY") == 1
            ? "us.lyt" : "fr-azerty.lyt";

    firstboot_step(3, "System region",
                   "Choose the region preference for the system.");
    sys_puts("  1) United States\n  2) France\n\n", 0x0E);
    settings->region =
        firstboot_choose("Region [1-2]: ", "United States", "France") == 1
            ? "US" : "FR";

    firstboot_step(4, "Time zone",
                   "Choose the time zone to store in system settings.");
    sys_puts("  1) UTC\n  2) Europe/Paris\n\n", 0x0E);
    settings->timezone =
        firstboot_choose("Time zone [1-2]: ", "UTC", "Europe/Paris") == 1
            ? "UTC" : "Europe/Paris";
}

static int firstboot_save_settings(const firstboot_settings_t* settings)
{
    char locale[96];
    int length = snprintf(locale, sizeof(locale),
                          "LANG=%s\nREGION=%s\n", settings->language,
                          settings->region);
    if (length < 0 || (size_t)length >= sizeof(locale) ||
        sys_file_replace("/etc/locale.conf", locale, (size_t)length) < 0) {
        firstboot_creation_log("Save /etc/locale.conf", 0);
        return 0;
    }
    firstboot_creation_log("Save language and region settings", 1);
    if (sys_file_replace("/etc/keyboards/default", settings->keyboard,
                        strlen(settings->keyboard)) < 0) {
        firstboot_creation_log("Save /etc/keyboards/default", 0);
        return 0;
    }
    firstboot_creation_log("Save keyboard layout", 1);
    if (sys_file_replace("/etc/timezone", settings->timezone,
                        strlen(settings->timezone)) < 0) {
        firstboot_creation_log("Save /etc/timezone", 0);
        return 0;
    }
    firstboot_creation_log("Save time-zone setting", 1);
    return 1;
}

/* Read password (hidden) */
static void firstboot_read_password(char* buf, int max) {
    int len = 0;
    buf[0] = 0;

    while (1) {
        char c = sys_getc();
        if (!c) continue;

        if (c == '\n' || c == '\r') {
            sys_putchar('\n');
            buf[len] = 0;
            return;
        }

        if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                buf[len] = 0;
                sys_putchar('\b');
                sys_putchar(' ');
                sys_putchar('\b');
            }
            continue;
        }

        if ((unsigned char)c < 0x20 || (unsigned char)c >= 0x7F)
            continue;

        if (len < max - 1) {
            buf[len++] = c;
            buf[len] = 0;
            sys_putchar('*');
        }
    }
}

void _start(void) {
    char username[32];
    char pass1[32];
    char pass2[32];
    int ret = 0;
    firstboot_settings_t settings;

    if (sys_user_count() > 0) {
        char* existing_argv[] = { "/bin/login", NULL };
        sys_exec("/bin/login", existing_argv);
        sys_exit(1);
    }

    firstboot_select_settings(&settings);
    firstboot_step(5, "Administrator account",
                   "Choose the initial account name for this computer.");
    printf("  Language: %s\n  Keyboard: %s\n  Region: %s\n"
           "  Time zone: %s\n\n",
           settings.language, settings.keyboard, settings.region,
           settings.timezone);
    sys_puts("  This account manages the system.\n\n", 0x0E);

    /* Ask for username */
    while (1) {
        sys_puts("Enter username: ", 0x0F);
        firstboot_readline(username, sizeof(username));

        if (username[0] == 0) {
            sys_puts("Username cannot be empty.\n\n", 0x0C);
            continue;
        }
        if (!user_name_creation_valid(username)) {
            sys_puts("Username must contain only letters (A-Z, a-z), with no digits or symbols.\n\n",
                     0x0C);
            continue;
        }

        /* Check if user already exists */
        user_t existing;
        int ret = sys_user_find(username, &existing);
        if (ret < 0) {
            sys_puts("Unable to verify whether the user exists.\n\n", 0x0C);
            continue;
        }
        if (ret > 0) {
            sys_puts("User already exists.\n\n", 0x0C);
            continue;
        }
        break;
    }

    firstboot_step(6, "Secure your administrator account",
                   "Choose a password. Input is hidden while you type.");
    printf("  Account name: %s\n\n", username);

    /* Ask for password */
    while (1) {
        sys_puts("New password: ", 0x0F);
        firstboot_read_password(pass1, sizeof(pass1));

        sys_puts("Confirm password: ", 0x0F);
        firstboot_read_password(pass2, sizeof(pass2));

        if (strcmp(pass1, pass2) != 0) {
            printf("\nPasswords do not match.\n\n");
            continue;
        }

        if (!pass1[0]) {
            printf("\nEmpty password not allowed.\n\n");
            continue;
        }

        sys_clear();
        sys_puts("============================================================\n",
                 0x0B);
        sys_puts("  CREATEOS  /  ACCOUNT CREATION LOG\n", 0x0F);
        sys_puts("============================================================\n\n",
                 0x0B);
        printf("  Account: %s  (administrator)\n\n", username);
        sys_puts("  [....] Saving account credentials\n", 0x0E);

        ret = sys_user_add(username, pass1, 3);
        if (ret <= 0) {
            firstboot_creation_log("Create administrator account", 0);
            if (ret == USER_ADD_NO_SECURE_RANDOM) {
                sys_puts("\nSecure random source unavailable; account not created.\n",
                         0x0C);
                sys_puts("QEMU: launch with -cpu max; on hardware enable RDSEED/RDRAND.\n\n",
                         0x0E);
                sys_exit(1);
            } else if (ret == USER_ADD_DATABASE_FAILED) {
                sys_puts("\nCannot save /etc/users.conf; check filesystem space and write access.\n\n",
                         0x0C);
            } else if (ret == USER_ADD_PERMISSION_DENIED) {
                sys_puts("\nInitial account creation is no longer permitted.\n\n",
                         0x0C);
                sys_exit(1);
            } else if (ret == USER_ADD_ALREADY_EXISTS) {
                sys_puts("\nThat username already exists; choose another one.\n\n",
                         0x0C);
            } else {
                sys_puts("\nFailed to create user (invalid name or account limit).\n\n",
                         0x0C);
            }
            sys_puts("\nPress Enter to retry with a new password.\n", 0x0E);
            {
                char retry[2];
                firstboot_readline(retry, sizeof(retry));
            }
            continue;
        }
        firstboot_creation_log("Save administrator account record", 1);

        user_t created_user;
        if (sys_user_current(&created_user) <= 0 ||
            !user_setup_home(&created_user)) {
            firstboot_creation_log("Create administrator home files", 0);
            sys_puts("\nThe account was saved, but its home setup failed. "
                     "The login program will retry setup.\n\n", 0x0C);
            sys_exit(1);
        }
        firstboot_creation_log("Create home configuration and app shortcuts",
                               1);
        if (sys_chdir(created_user.home) < 0) {
            firstboot_creation_log("Select administrator home directory", 0);
            sys_puts("\nThe account and home files were saved. Login will "
                     "select the home directory on the next boot.\n\n", 0x0C);
            sys_exit(1);
        }
        firstboot_creation_log("Select administrator home directory", 1);
        break;
    }

    if (!firstboot_save_settings(&settings)) {
        sys_puts("\nSetup is incomplete: one or more selected settings could "
                 "not be saved.\n", 0x0C);
        sys_puts("The account and home files were saved, but the completion "
                 "marker was not written.\n", 0x0E);
        sys_puts("Sign in and use Settings to save the missing preferences, "
                 "then restart if needed.\n", 0x0E);
        sys_exit(1);
    }

    /* Mark first boot as done */
    if (sys_open("/etc/.firstboot_done", O_CREAT | O_WRONLY, 0644) < 0 ||
        sys_write_path("/etc/.firstboot_done", "1", 1) != 1) {
        firstboot_creation_log("Write first-boot completion marker", 0);
        sys_puts("Setup succeeded, but the first-boot marker could not be saved.\n",
                 0x0C);
        sys_puts("The existing account will still be used on the next boot.\n",
                 0x0E);
    } else {
        firstboot_creation_log("Write first-boot completion marker", 1);
    }

    if (sys_sync() < 0) {
        sys_puts("\nSetup complete, but storage synchronization failed. "
                 "Automatic restart was cancelled.\n", 0x0C);
    } else {
        sys_puts("\nSetup complete. Restarting CreateOS...\n", 0x0A);
        sys_exit(0xDEAD);
        sys_puts("The restart request did not complete.\n", 0x0C);
    }
    sys_puts("Starting the login service; run `reboot` after resolving the issue.\n",
             0x0E);
    {
        char* argv[] = { "/sbin/init", 0 };
        if (sys_exec("/sbin/init", argv) < 0)
            sys_puts("System init startup failed. Halting safely.\n", 0x0C);
    }
    sys_exit(1);
}