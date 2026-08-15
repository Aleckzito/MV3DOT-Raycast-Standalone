#ifndef DATA_PATHS_H
#define DATA_PATHS_H

#include <string>

namespace rc {

std::string findDataFile(const std::string& relativePath);
std::string findRepoRoot();

} // namespace rc

#endif
