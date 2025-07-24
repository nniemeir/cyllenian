#include "file.h"
#include "log.h"

bool file_exists(const char *filename) {
  struct stat buffer;
  int result = stat(filename, &buffer);
  if (result == -1) {
    return false;
  }
  return true;
}

char *get_file_extension(const char *file_path) {
  char *file_extension = strrchr(file_path, '.');
  if (!file_extension) {
    return NULL;
  }
  file_extension++;
  return file_extension;
}

unsigned char *read_file(const char *file_path, size_t *file_size) {
  FILE *file = fopen(file_path, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open file %s:%s\n", file_path, strerror(errno));
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) == -1) {
    fprintf(stderr, "Failed to set file position indicator: %s\n",
            strerror(errno));
    fclose(file);
    return NULL;
  }

  int size = ftell(file);
  if (size == -1) {
    fprintf(stderr, "Failed to get file position indicator: %s\n",
            strerror(errno));
    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
      return NULL;
    }
  }

  *file_size = (size_t)size;

  rewind(file);

  unsigned char *buffer = (unsigned char *)malloc(*file_size);
  if (!buffer) {
    fprintf(stderr, "Failed to allocate memory for file buffer: %s\n",
            strerror(errno));

    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
    }
    return NULL;
  }

  const size_t bytes_read = fread(buffer, 1, *file_size, file);

  if (bytes_read != *file_size) {
    log_event(ERROR, "Error reading file into buffer.");
    free(buffer);

    if (fclose(file) == -1) {
      fprintf(stderr, "Failed to close file %s:%s\n", file_path,
              strerror(errno));
    }

    return NULL;
  }

  if (fclose(file) == -1) {
    fprintf(stderr, "Failed to close file %s:%s\n", file_path, strerror(errno));
    return NULL;
  }

  return buffer;
}
