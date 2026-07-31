#pragma once

#include <QStringList>

namespace Elevate {

bool isAdmin();
/** 非管理员则 ShellExecute runas 重启；成功发起返回 true（调用方应退出）。 */
bool relaunchAsAdmin(const QStringList &extraArgs = {});
/** 启动时提权；若已发起管理员进程返回 true。用户取消则返回 false 继续普通权限。 */
bool ensureAdminAtStart(int argc, char *argv[]);

} // namespace Elevate
