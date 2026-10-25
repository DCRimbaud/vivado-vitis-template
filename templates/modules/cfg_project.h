//////////////////////////////////////////////////////////////////////////////////
// SPDX-License-Identifier: MIT
// Author: Xue Weisen <xueweisen96@gmail.com>
// GitHub: https://github.com/DCRimbaud
// Create Date: __DATE__
// Design Name: __PROJECT__
// Module Name: cfg_project
// Target Devices: __DEVICE__
// Tool Versions: __TOOL_VERSIONS__
// Description: 项目版本与构建基础配置
// Revision: 0.01
// Revision 0.01 - File Created
// Keywords: project-identity, firmware-version, build-id, protocol-schema, single-source
//////////////////////////////////////////////////////////////////////////////////
#ifndef CFG_PROJECT_H
#define CFG_PROJECT_H

#define CFG_PROJECT_NAME      "__PROJECT__"
#define CFG_FW_VERSION        "0.1.0"
#define CFG_BUILD_ID          0u
#define CFG_PROTOCOL_SCHEMA   1u

_Static_assert(CFG_PROTOCOL_SCHEMA > 0u, "protocol schema must be positive");

#endif /* CFG_PROJECT_H */
