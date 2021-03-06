LIBS      = base blit

SRC_CC    = sky_texture.cc     startup.cc   \
            elements.cc        widgets.cc   \
            tick.cc            scrollbar.cc \
            refracted_icon.cc

SRC_CC   += platform_genode.cc

SCOUT_DIR = $(REP_DIR)/src/app/scout

INC_DIR  += $(SCOUT_DIR)/include \
            $(SCOUT_DIR)/include/genode

vpath %                  $(SCOUT_DIR)/data
vpath %.cc               $(SCOUT_DIR)/common
vpath startup.cc         $(SCOUT_DIR)/genode
vpath launcher.cc        $(SCOUT_DIR)/genode
vpath platform_genode.cc $(SCOUT_DIR)/genode


SRC_TFF   =   vera16.tff \
             verai16.tff \
              vera18.tff \
              vera20.tff \
              vera24.tff \
            verabi10.tff \
              mono16.tff

SRC_RGBA  =   uparrow.rgba \
            downarrow.rgba \
               slider.rgba \
                sizer.rgba \
             titlebar.rgba \
              loadbar.rgba \
               redbar.rgba \
             whitebar.rgba \
            kill_icon.rgba \
          opened_icon.rgba \
          closed_icon.rgba

SRC_BIN = $(SRC_TFF) $(SRC_MAP) $(SRC_RGBA)
