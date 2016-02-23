/*
 * \brief  Linux framebuffer device file system
 * \author Martin Stein
 * \date   2016-02-09
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__VFS__LXFB_FILE_SYSTEM_H_
#define _INCLUDE__VFS__LXFB_FILE_SYSTEM_H_

#include <vfs/lxfb.h>
#include <vfs/single_file_system.h>
#include <framebuffer_session/connection.h>

namespace Vfs { class Lxfb_file_system; }

class Vfs::Lxfb_file_system : public Single_file_system
{
	private:

		enum {
			LXFB_WIDTH  = 640,
			LXFB_HEIGHT = 480,
			LXFB_BPP    = 32,
		};

		Framebuffer::Connection    _fb;

		Ioctl_result _fbioget_fscreeninfo(Ioctl_arg arg)
		{
			using Genode::size_t;
			Framebuffer::Mode fb_mode = _fb.mode();
			size_t fb_line_sz = fb_mode.width() * fb_mode.bytes_per_pixel();
			size_t fb_sz      = fb_line_sz * fb_mode.height();

			fb_fix_screeninfo * fixinfo = (fb_fix_screeninfo *)arg;
			fixinfo->smem_start  = 0;
			fixinfo->smem_len    = fb_sz;
			fixinfo->line_length = fb_line_sz;
			return IOCTL_OK;
		}

		Ioctl_result _fbioget_vscreeninfo(Ioctl_arg arg)
		{
			Framebuffer::Mode fb_mode = _fb.mode();
			fb_var_screeninfo * varinfo = (fb_var_screeninfo *)arg;
			varinfo->xres = fb_mode.width();
			varinfo->yres = fb_mode.height();
			varinfo->xoffset = 0;
			varinfo->yoffset = 0;
			varinfo->bits_per_pixel = fb_mode.bytes_per_pixel() * 8;
			switch (fb_mode.format()) {
			case Framebuffer::Mode::RGB565:
				varinfo->blue.offset  = 0;
				varinfo->green.offset = 5;
				varinfo->red.offset   = 11;
				break;
			default:
				PERR("invalid framebuffer format");
				return IOCTL_ERR_INVALID;
			}
			return IOCTL_OK;
		}

	public:

		Lxfb_file_system(Xml_node config)
		:
			Single_file_system(NODE_TYPE_CHAR_DEVICE, name(), config)
		{ }

		static const char *name() { return "lxfb"; }


		/********************************
		 ** File I/O service interface **
		 ********************************/

		Write_result write(Vfs_handle *, char const *src, file_size count,
		                   file_size &out_count) override
		{
			PDBG("Not implemented");
			return WRITE_ERR_INVALID;
		}

		Read_result read(Vfs_handle *vfs_handle, char *dst, file_size count,
		                 file_size &out_count) override
		{
			PDBG("Not implemented");
			return READ_ERR_INVALID;
		}

		Ioctl_result ioctl(Vfs_handle *vfs_handle, Ioctl_opcode opcode,
		                   Ioctl_arg arg, Ioctl_out &out) override
		{
			switch (opcode) {
			case IOCTL_OP_FBIOGET_VSCREENINFO: return _fbioget_vscreeninfo(arg);
			case IOCTL_OP_FBIOGET_FSCREENINFO: return _fbioget_fscreeninfo(arg);
			default: PDBG("invalid ioctl request %d", opcode); break;
			}
			return IOCTL_ERR_INVALID;
		}

		Mmap_type mmap_type() override { return MMAP_DIRECT; }

		Dataspace_capability mmap_direct_ds() override { return _fb.dataspace(); }
};

#endif /* _INCLUDE__VFS__LXFB_FILE_SYSTEM_H_ */
