#pragma once

#include <cstddef>
#include <cstdio>
class H264File {
public:
    H264File(int buffersize = 500000);
    ~H264File();
    bool Open(const char* path);
    void Close();

    bool IsOpen() const {
        return m_file != NULL;
    }

    int ReadFrame(char *in_buf, int in_buf_size, bool *end);

private:
    FILE *m_file = NULL;
    char *m_buf = nullptr;
    int m_buf_size = 0;
    int m_bytes_used = 0;
    int m_count = 0;
};