#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "pixel_format.hpp"
#include "shm.hpp"

extern "C"{
	#include <assert.h>
	#include <drm_fourcc.h>
	#include <stdlib.h>
	#include <wlr/interfaces/wlr_buffer.h>
	#include <wlr/render/allocator.h>
	#include <wlr/render/drm_format_set.h>
	#include <wlr/util/log.h>
}

#define RANDNAME_PATTERN "/wlshm-XXXXXX"

static void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int excl_shm_open(char *name) {
	int retries = 100;
	do {
		randname(name + strlen(RANDNAME_PATTERN) - 6);

		--retries;
		// CLOEXEC is guaranteed to be set by shm_open
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);

	return -1;
}

int allocate_shm_file(size_t size) {
	char name[] = RANDNAME_PATTERN;

	int fd = excl_shm_open(name);
	if (fd < 0) {
		return -1;
	}
	shm_unlink(name);
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static struct wlr_shm_buffer *shm_buffer_from_buffer(struct wlr_buffer *wlr_buffer);

static void buffer_destroy(struct wlr_buffer *wlr_buffer); 

static bool buffer_get_shm(struct wlr_buffer *wlr_buffer, struct wlr_shm_attributes *shm);

static bool shm_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride);

static void shm_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl buffer_impl = {
	.destroy = buffer_destroy,
	.get_shm = buffer_get_shm,
	.begin_data_ptr_access = shm_buffer_begin_data_ptr_access,
	.end_data_ptr_access = shm_buffer_end_data_ptr_access,
};

static struct wlr_shm_buffer *shm_buffer_from_buffer(
		struct wlr_buffer *wlr_buffer) {
	assert(wlr_buffer->impl == &buffer_impl);
	struct wlr_shm_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	return buffer;
}

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_shm_buffer *buffer = shm_buffer_from_buffer(wlr_buffer);
	munmap(buffer->data, buffer->size);
	close(buffer->shm.fd);
	free(buffer);
}

static bool buffer_get_shm(struct wlr_buffer *wlr_buffer,
		struct wlr_shm_attributes *shm) {
	struct wlr_shm_buffer *buffer = shm_buffer_from_buffer(wlr_buffer);
	*shm = buffer->shm;
	return true;
}

static bool shm_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wlr_shm_buffer *buffer = shm_buffer_from_buffer(wlr_buffer);
	*data = buffer->data;
	*format = buffer->shm.format;
	*stride = buffer->shm.stride;
	return true;
}


static struct wlr_buffer *allocator_create_buffer(
		struct wlr_allocator *wlr_allocator, int width, int height,
		const struct wlr_drm_format *format) {
	const struct wlr_pixel_format_info *info =
		drm_get_pixel_format_info(format->format);
	if (info == NULL) {
		wlr_log(WLR_ERROR, "Unsupported pixel format 0x%" PRIX32, format->format);
		return NULL;
	}

	struct wlr_shm_buffer *buffer = (struct wlr_shm_buffer*)calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &buffer_impl, width, height);

	// TODO: consider using a single file for multiple buffers
	int stride = pixel_format_info_min_stride(info, width); // TODO: align?
	buffer->size = stride * height;
	buffer->shm.fd = allocate_shm_file(buffer->size);
	if (buffer->shm.fd < 0) {
		free(buffer);
		return NULL;
	}

	buffer->shm.format = format->format;
	buffer->shm.width = width;
	buffer->shm.height = height;
	buffer->shm.stride = stride;
	buffer->shm.offset = 0;

	buffer->data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		buffer->shm.fd, 0);
	if (buffer->data == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "mmap failed");
		close(buffer->shm.fd);
		free(buffer);
		return NULL;
	}

	return &buffer->base;
}

static void allocator_destroy(struct wlr_allocator *wlr_allocator) {
	free(wlr_allocator);
}

static const struct wlr_allocator_interface allocator_impl = {
	.create_buffer = allocator_create_buffer,
	.destroy = allocator_destroy,
};

struct wlr_allocator *wlr_shm_allocator_create(void) {
	struct wlr_shm_allocator *allocator = (struct wlr_shm_allocator*)calloc(1, sizeof(*allocator));
	if (allocator == NULL) {
		return NULL;
	}
	wlr_allocator_init(&allocator->base, &allocator_impl,
		WLR_BUFFER_CAP_DATA_PTR | WLR_BUFFER_CAP_SHM);

	wlr_log(WLR_DEBUG, "Created shm allocator");
	return &allocator->base;
}
