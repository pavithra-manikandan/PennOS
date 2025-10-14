#include "./Vec.h"
#include <stdlib.h>
#include "./util/panic.h"

Vec vec_new(size_t initial_capacity, ptr_dtor_fn ele_dtor_fn) {
  Vec new_vec;
  new_vec.data = malloc(initial_capacity * sizeof(ptr_t));
  if (new_vec.data == NULL) {
    panic("Memory allocation failed");
  }
  new_vec.length = 0;
  if (initial_capacity < 0) {
    panic("Initial capacity must be non-negative");
  }
  new_vec.capacity = initial_capacity;
  new_vec.ele_dtor_fn = ele_dtor_fn;
  return new_vec;
}

ptr_t vec_get(Vec* self, size_t index) {
  if (index >= vec_len(self)) {
    panic("Index out of bounds");
  }
  return self->data[index];
}

void vec_set(Vec* self, size_t index, ptr_t new_ele) {
  if (index >= vec_len(self)) {
    panic("Index out of bounds");
  } else {
    if (self->ele_dtor_fn != NULL) {
      self->ele_dtor_fn(self->data[index]);
    }
    self->data[index] = new_ele;
  }
}

void vec_push_back(Vec* self, ptr_t new_ele) {
  if (vec_len(self) == vec_capacity(self)) {
    size_t new_capacity = vec_capacity(self) == 0 ? 1 : vec_capacity(self) * 2;
    vec_resize(self, new_capacity);
  }
  self->data[vec_len(self)] = new_ele;
  self->length++;
}

bool vec_pop_back(Vec* self) {
  if (vec_len(self) == 0) {
    return false;
  }
  if (self->ele_dtor_fn != NULL) {
    self->ele_dtor_fn(self->data[vec_len(self) - 1]);
  }
  self->length--;
  return true;
}

void vec_insert(Vec* self, size_t index, ptr_t new_ele) {
  if (index > vec_len(self)) {
    panic("Index out of bounds");
  }
  if (vec_len(self) == vec_capacity(self)) {
    size_t new_capacity = vec_capacity(self) == 0 ? 1 : vec_capacity(self) * 2;
    vec_resize(self, new_capacity);
  }
  for (size_t i = vec_len(self); i > index; i--) {
    self->data[i] = self->data[i - 1];
  }
  self->data[index] = new_ele;
  self->length++;
}

void vec_erase(Vec* self, size_t index) {
  if (index >= vec_len(self)) {
    panic("Index out of bounds");
  }
  if (self->ele_dtor_fn != NULL) {
    self->ele_dtor_fn(self->data[index]);
  }
  for (size_t i = index; i < vec_len(self) - 1; i++) {
    self->data[i] = self->data[i + 1];
  }
  self->length--;
}

void vec_shallow_erase(Vec* self, size_t index) {
  if (index >= vec_len(self)) {
    panic("Index out of bounds");
  }
  for (size_t i = index; i < vec_len(self) - 1; i++) {
    self->data[i] = self->data[i + 1];
  }
  self->length--;
}

void vec_resize(Vec* self, size_t new_capacity) {
  if (new_capacity > vec_capacity(self)) {
    ptr_t* new_data = malloc(new_capacity * sizeof(ptr_t));
    if (new_data == NULL) {
      panic("Memory allocation failed");
    }
    for (size_t i = 0; i < vec_len(self); i++) {
      new_data[i] = self->data[i];
    }
    free(self->data);
    self->data = new_data;
    self->capacity = new_capacity;
  }
}

void vec_clear(Vec* self) {
  if (self->ele_dtor_fn != NULL) {
    for (size_t i = 0; i < vec_len(self); i++) {
      self->ele_dtor_fn(self->data[i]);
      self->data[i] = NULL;
    }
  }
  self->length = 0;
}

void vec_destroy(Vec* self) {
  vec_clear(self);
  free(self->data);
  self->data = NULL;
  self->length = 0;
  self->capacity = 0;
  self->ele_dtor_fn = NULL;
}
