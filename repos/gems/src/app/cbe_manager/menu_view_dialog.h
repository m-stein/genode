/*
 * \brief  Local utilities for the menu view dialog
 * \author Martin Stein
 * \date   2021-02-24
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MENU_VIEW_DIALOG_H_
#define _MENU_VIEW_DIALOG_H_

/* Genode includes */
#include <util/xml_generator.h>

/* local includes */
#include <types.h>

namespace Cbe_manager {

	void gen_frame_title(Xml_generator &xml,
	                     char    const *title,
	                     char    const *name,
	                     unsigned long  min_width);

	template <typename GEN_FRAME_CONTENT>
	void gen_titled_frame(Xml_generator           &xml,
	                      char              const *name,
	                      char              const *title,
	                      unsigned long            min_width,
	                      GEN_FRAME_CONTENT const &gen_frame_content)
	{
		xml.node("frame", [&] () {
			xml.attribute("name", name);

			xml.node("vbox", [&] () {

				gen_frame_title(xml, title, "title", min_width);
				gen_frame_content(xml);
			});
		});
	}

	void gen_titled_in_progress_frame(Xml_generator &xml,
	                                  char const    *name,
	                                  char const    *title,
	                                  unsigned long  min_width);

	void gen_action_button_at_bottom(Xml_generator &xml,
	                                 char const    *name,
	                                 char const    *label,
	                                 bool           hovered,
	                                 bool           selected);

	void gen_action_button_at_bottom(Xml_generator &xml,
	                                 char const    *label,
	                                 bool           hovered,
	                                 bool           selected);

	void gen_titled_text_input(Xml_generator     &xml,
	                           char        const *name,
	                           char        const *title,
	                           String<256> const &text,
                               bool               selected);

	void gen_floating_text_line(Xml_generator &xml,
	                            char    const *name,
	                            char    const *line,
	                            unsigned long  select_at = 0,
	                            unsigned long  select_length = 0);

	void gen_info_line(Xml_generator     &xml,
	                   char        const *name,
	                   char        const *text);

	template <typename GEN_FLOATING_TEXT>
	void gen_floating_text_frame(Xml_generator           &xml,
	                             char              const *name,
	                             GEN_FLOATING_TEXT const &gen_floating_text)
	{
		xml.node("frame", [&] () {
			xml.attribute("name", name);

			xml.node("float", [&] () {
				xml.attribute("north", "yes");
				xml.attribute("east",  "yes");
				xml.attribute("west",  "yes");

				xml.node("vbox", [&] () {
					gen_floating_text(xml);
					gen_floating_text_line(xml, "pad", "");
				});
			});
		});
	}
}

#endif /* _MENU_VIEW_DIALOG_H_ */
