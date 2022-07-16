/**
 * Copyright 2022 Ketan Goyal
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BIGCAT_MQ__DETAILS__SHARED_OBJECT_HPP
#define BIGCAT_MQ__DETAILS__SHARED_OBJECT_HPP

#include <sys/mman.h>
#include <sys/stat.h> /* For mode constants */
#include <fcntl.h>    /* For O_* constants */

#include <cerrno>
#include <string>
#include <system_error>
#include <type_traits>

namespace bigcat {
namespace details {

template <class T>
class SharedObject {
  static_assert(std::is_trivial<T>::value,
                "The data type used does not have a trivial memory layout.");

 public:
  template <class... Args>
  static T* Instance(const std::string& name, Args&&... args);
  static bool Remove(const std::string& name);

 private:
  template <class... Args>
  SharedObject(const std::string& name, Args&&... args);

  const std::string name_;
  T* object_;
};

// --------------------------------
// SharedObject Implementation
// --------------------------------

template <class T>
template <class... Args>
SharedObject<T>::SharedObject(const std::string& name, Args&&... args)
    : name_(name), object_(nullptr) {
  auto fd =
      shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  if (fd == -1 && errno != EEXIST) {
    throw std::system_error(errno, std::generic_category(), "shm_open");
  } else if (fd == -1 && errno == EEXIST) {
    fd = shm_open(name.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  } else {
    if (ftruncate(fd, sizeof(T)) == -1) {
      throw std::system_error(errno, std::generic_category(), "ftruncate");
    }
  }
  auto addr =
      mmap(nullptr, sizeof(T), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    throw std::system_error(errno, std::generic_category(), "mmap");
  }
  close(fd);

  object_ = reinterpret_cast<T*>(addr);
}

// ----------- public -------------

// static
template <class T>
template <class... Args>
T* SharedObject<T>::Instance(const std::string& name, Args&&... args) {
  static SharedObject instance(name, std::forward<Args>(args)...);
  return instance.object_;
}

// static
template <class T>
bool SharedObject<T>::Remove(const std::string& name) {
  return shm_unlink(name.c_str()) < 0 ? false : true;
}

// --------------------------------

}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__SHARED_OBJECT_HPP */
