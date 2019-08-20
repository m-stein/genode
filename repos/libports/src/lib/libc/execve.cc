/*
 * \brief  Libc execve mechanism
 * \author Norman Feske
 * \date   2019-08-20
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/shared_object.h>
#include <base/log.h>

/* libc includes */
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

/* libc-internal includes */
#include <libc/allocator.h>
#include <internal/call_func.h>
#include <libc_init.h>
#include <task.h>

using namespace Genode;

typedef int (*main_fn_ptr) (int, char **, char **);

namespace Libc { struct Env_vars; }

struct Libc::Env_vars : Noncopyable
{
	typedef Genode::Allocator Allocator;

	Allocator &_alloc;

	static unsigned _num_entries(char const * const * const environ)
	{
		unsigned i = 0;
		for (; environ && environ[i]; i++);
		return i;
	}

	unsigned const _count;

	size_t const _environ_size = sizeof(char *)*(_count + 1);

	char ** const environ = (char **)_alloc.alloc(_environ_size);

	struct Buffer
	{
		Allocator &_alloc;
		size_t const _size;
		char * const _base = (char *)_alloc.alloc(_size);

		unsigned _pos = 0;

		Buffer(Allocator &alloc, size_t size)
		: _alloc(alloc), _size(size) { }

		~Buffer() { _alloc.free(_base, _size); }

		bool try_append(char const *s)
		{
			size_t const len = ::strlen(s) + 1;
			if (_pos + len > _size)
				return false;

			Genode::strncpy(_base + _pos, s, len);
			_pos += len;
			return true;
		}

		char *pos_ptr() { return _base + _pos; }
	};

	Constructible<Buffer> _buffer { };

	Env_vars(Allocator &alloc, char const * const * const src_environ)
	:
		_alloc(alloc),
		_count(_num_entries(src_environ))
	{
		/* marshal environ strings to buffer */
		size_t size = 1024;
		for (;;) {

			_buffer.construct(alloc, size);

			unsigned i = 0;
			for (i = 0; i < _count; i++) {
				environ[i] = _buffer->pos_ptr();
				if (!_buffer->try_append(src_environ[i]))
					break;
			}

			bool const done = (i == _count);
			if (done) {
				environ[i] = nullptr;
				break;
			}

			warning("env buffer ", size, " too small");
			size += 1024;
		}
	}

	~Env_vars() { _alloc.free(environ, _environ_size); }
};


/* pointer to environment, provided by libc */
extern char **environ;

enum { MAX_ARGS = 256 };

static Env                     *_env_ptr;
static Allocator               *_alloc_ptr;
static void                    *_user_stack_ptr;
static int                      _argc;
static char                    *_argv[MAX_ARGS];
static main_fn_ptr              _main_ptr;
static Libc::Env_vars          *_env_vars_ptr;
static Libc::Reset_malloc_heap *_reset_malloc_heap_ptr;


void Libc::init_execve(Env &env, Genode::Allocator &alloc, void *user_stack_ptr,
                       Reset_malloc_heap &reset_malloc_heap)
{
	_env_ptr               = &env;
	_alloc_ptr             = &alloc;
	_user_stack_ptr        =  user_stack_ptr;
	_reset_malloc_heap_ptr = &reset_malloc_heap;

	Dynamic_linker::keep(env, "libc.lib.so");
	Dynamic_linker::keep(env, "libm.lib.so");
	Dynamic_linker::keep(env, "posix.lib.so");
	Dynamic_linker::keep(env, "vfs.lib.so");
}


static void user_entry(void *)
{
	_env_ptr->exec_static_constructors();

	exit((*_main_ptr)(_argc, _argv, _env_vars_ptr->environ));
}


extern "C" int execve(char const *, char *const[], char *const[]) __attribute__((weak));

extern "C" int execve(char const *filename,
                      char *const argv[], char *const envp[])
{
	if (!_env_ptr || !_alloc_ptr) {
		error("missing call of 'init_execve'");
		errno = EACCES;
		return -1;
	}

	/* capture environment variables to libc-internal heap */
	Libc::Env_vars *saved_env_vars = new (*_alloc_ptr) Libc::Env_vars(*_alloc_ptr, envp);

	try {
		_main_ptr = Dynamic_linker::respawn<main_fn_ptr>(*_env_ptr, filename, "main");
	}
	catch (Dynamic_linker::Invalid_symbol) {
		error("Dynamic_linker::respawn could not obtain binary entry point");
		errno = EACCES;
		return -1;
	}
	catch (Dynamic_linker::Invalid_rom_module) {
		error("Dynamic_linker::respawn could not access binary ROM");
		errno = EACCES;
		return -1;
	}

	/*
	 * Copy argv from application-owned (malloc heap) into libc-owned memory
	 *
	 * XXX
	 */
	for (unsigned i = 0; i < MAX_ARGS; i++) {
		if (_argv[i]) {
			_alloc_ptr->free(_argv[i], ::strlen(_argv[i]) + 1);
			_argv[i] = nullptr;
		}
	}

	{
		unsigned i = 0;
		for (i = 0; i < MAX_ARGS && argv[i]; i++) {
			size_t const len = ::strlen(argv[i]) + 1; /* zero termination */
			_argv[i] = (char *)_alloc_ptr->alloc(len);
			Genode::strncpy(_argv[i], argv[i], len);
		}
		if (i == MAX_ARGS) {
			errno = E2BIG;
			return -1;
		}
		_argc = i;
	}

	/*
	 * Reconstruct malloc heap for application-owned data
	 */
	_reset_malloc_heap_ptr->reset_malloc_heap();

	Libc::Allocator app_heap { };

	_env_vars_ptr = new (app_heap) Libc::Env_vars(app_heap, saved_env_vars->environ);

	/* register list of environment variables at libc 'environ' pointer */
	environ = _env_vars_ptr->environ;

	destroy(_alloc_ptr, saved_env_vars);

	call_func(_user_stack_ptr, (void *)user_entry, nullptr);
}

