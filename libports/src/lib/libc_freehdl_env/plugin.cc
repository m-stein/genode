
class Plugin : public Libc::Plugin
{
	public:

		/**
		 * Constructor
		 */
		Plugin() { }

		~Plugin() { }

		bool supports_chdir(const char *path)
		{
			if (verbose) PDBG("path = %s", path);
			return 0;
		}

		bool supports_mkdir(const char *path, mode_t)
		{
			if (verbose) PDBG("path = %s", path);
			return 0;
		}

		bool supports_open(const char *pathname, int flags)
		{
			if (verbose) PDBG("pathname = %s", pathname);
			return 0;
		}

		bool supports_rename(const char *oldpath, const char *newpath)
		{
			if (verbose) PDBG("oldpath = %s, newpath = %s", oldpath, newpath);
			return 0;
		}

		bool supports_stat(const char *path)
		{
			if (verbose) PDBG("path = %s", path);
			return 0;
		}

		bool supports_unlink(const char *path)
		{
			if (verbose) PDBG("path = %s", path);
			return 0;
		}

		int chdir(const char *path)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int close(Libc::File_descriptor *fd)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int fcntl(Libc::File_descriptor *, int cmd, long arg)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int fstat(Libc::File_descriptor *fd, struct stat *buf)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int fstatfs(Libc::File_descriptor *, struct statfs *buf)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int fsync(Libc::File_descriptor *fd)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		ssize_t getdirentries(Libc::File_descriptor *fd, char *buf,
		                      ::size_t nbytes, ::off_t *basep)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		::off_t lseek(Libc::File_descriptor *fd, ::off_t offset, int whence)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int mkdir(const char *path, mode_t mode)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		Libc::File_descriptor *open(const char *pathname, int flags)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int rename(const char *oldpath, const char *newpath)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		ssize_t read(Libc::File_descriptor *fd, void *buf, ::size_t count)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int stat(const char *pathname, struct stat *buf)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		int unlink(const char *path)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}

		ssize_t write(Libc::File_descriptor *fd, const void *buf, ::size_t count)
		{
			PDBG("%s:%d: not implemented", __FILE__, __LINE__);
			while (1) ;
			return -1;
		}
};


} /* unnamed namespace */


void __attribute__((constructor)) init_libc_fs(void)
{
	PDBG("using the libc_fs plugin");
	static Plugin plugin;
}
