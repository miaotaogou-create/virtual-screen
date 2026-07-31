#include "Elevate.h"

#include <QCoreApplication>
#include <QString>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace Elevate {

bool isAdmin()
{
    BOOL admin = FALSE;
    PSID group = nullptr;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(nullptr, group, &admin);
        FreeSid(group);
    }
    return admin == TRUE;
}

bool relaunchAsAdmin(const QStringList &extraArgs)
{
    const QString exe = QCoreApplication::applicationFilePath();
    QString params;
    for (const QString &a : extraArgs) {
        if (!params.isEmpty())
            params += QLatin1Char(' ');
        if (a.contains(QLatin1Char(' ')))
            params += QLatin1Char('"') + a + QLatin1Char('"');
        else
            params += a;
    }
    const auto rc = reinterpret_cast<intptr_t>(
        ShellExecuteW(nullptr, L"runas",
                      reinterpret_cast<LPCWSTR>(exe.utf16()),
                      params.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(params.utf16()),
                      nullptr, SW_SHOWNORMAL));
    return rc > 32;
}

bool ensureAdminAtStart(int argc, char *argv[])
{
    if (isAdmin())
        return false;
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    return relaunchAsAdmin(args);
}

} // namespace Elevate
