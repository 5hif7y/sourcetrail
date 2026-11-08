#ifndef LIBSOURCETRAIL_HPP
#define LIBSOURCETRAIL_HPP

#include "libsourcetrail.h"
#include <string>
#include <stdexcept>
#include <memory>

namespace sourcetrail {

inline void init() {
    srctrl_init();
}

class Project {
public:
    explicit Project(const std::string& projectFilePath) {
        char* error_msg = nullptr;
        m_handle = srctrl_project_load(projectFilePath.c_str(), &error_msg);
        if (!m_handle) {
            std::string err = error_msg ? error_msg : "Unknown error";
            if (error_msg) {
                srctrl_free_string(error_msg);
            }
            throw std::runtime_error("Failed to load project: " + err);
        }
    }

    ~Project() {
        if (m_handle) {
            srctrl_project_free(m_handle);
            m_handle = nullptr;
        }
    }

    // Disable copy
    Project(const Project&) = delete;
    Project& operator=(const Project&) = delete;

    // Enable move
    Project(Project&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = nullptr;
    }
    Project& operator=(Project&& other) noexcept {
        if (this != &other) {
            if (m_handle) {
                srctrl_project_free(m_handle);
            }
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    int getNodeCount() const {
        return srctrl_project_get_node_count(m_handle);
    }

    int getEdgeCount() const {
        return srctrl_project_get_edge_count(m_handle);
    }

    // Expose the raw C handle if needed by advanced users
    srctrl_project getHandle() const {
        return m_handle;
    }

private:
    srctrl_project m_handle = nullptr;
};

} // namespace sourcetrail

#endif // LIBSOURCETRAIL_HPP
