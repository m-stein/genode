
/* base includes */
#include <util/xml_generator.h>

/* os includes */
#include <vfs/file_system_factory.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>

namespace Vfs_cbe_trust_anchor {

	class Plugin;
	class Root_dir_file_system;
	class Internal_file_system_factory;
	class Frontend_file_system_factory;

	using Storage_dir = Genode::String<256>;
}


class Vfs_cbe_trust_anchor::Plugin
{
	private:

		Vfs::Env          &_env;
		Storage_dir const  _storage_dir;

	public:

		Plugin(Vfs::Env          &env,
		       Storage_dir const &storage_dir);
};


class Vfs_cbe_trust_anchor::Internal_file_system_factory
:
	public Vfs::File_system_factory
{
	private:

		Plugin                                            _plugin;
		Vfs::Readonly_value_file_system<Genode::uint64_t> _responses_fs { "responses", 0 };

		static Storage_dir _storage_dir(Genode::Xml_node const &node);

	public:

		Internal_file_system_factory(Vfs::Env               &env,
		                             Genode::Xml_node const &node);

		Vfs::File_system *create(Vfs::Env         &env,
		                         Genode::Xml_node  node) override;
};


class Vfs_cbe_trust_anchor::Root_dir_file_system
:
	private Internal_file_system_factory,
	public  Vfs::Dir_file_system
{
	private:

		using Config = Genode::String<128>;

		static Config _config(Genode::Xml_node const &node);

	public:

		Root_dir_file_system(Vfs::Env         &env,
		                     Genode::Xml_node  node);

		char const *type() override { return "cbe_nta"; }
};


class Vfs_cbe_trust_anchor::Frontend_file_system_factory
:
	public Vfs::File_system_factory
{
	public:

		Vfs::File_system *create(Vfs::Env         &env,
		                         Genode::Xml_node  node) override;
};


using namespace Genode;
using namespace Vfs_cbe_trust_anchor;


/************
 ** Plugin **
 ************/

Plugin::Plugin(Vfs::Env          &env,
               Storage_dir const &storage_dir)
:
	_env         { env },
	_storage_dir { storage_dir }
{ }


/**********************************
 ** Internal_file_system_factory **
 **********************************/

Storage_dir Internal_file_system_factory::_storage_dir(Xml_node const &node)
{
	if (!node.has_attribute("storage_dir")) {

		class Missing_attribute { };
		throw Missing_attribute();
	}
	return node.attribute_value("storage_dir", Storage_dir { });
}


Vfs::File_system *Internal_file_system_factory::create(Vfs::Env &,
                                                       Xml_node  node)
{
	if (node.has_type("responses")) {
		return &_responses_fs;
	}
	return nullptr;
}


Internal_file_system_factory::Internal_file_system_factory(Vfs::Env       &env,
                                                           Xml_node const &node)
:
	_plugin { env, _storage_dir(node).string() }
{ }


/**************************
 ** Root_dir_file_system **
 **************************/

Root_dir_file_system::Config
Root_dir_file_system::_config(Genode::Xml_node const &node)
{
	char buf[Config::capacity()] { };

	Genode::Xml_generator xml(buf, sizeof(buf), "compound", [&] () {
		xml.node("dir", [&] () {
			xml.attribute("name", node.attribute_value("name", String<32>("")));
			xml.node("responses", [&] () {});
		});
	});

	return Config(Genode::Cstring(buf));
}


Root_dir_file_system::Root_dir_file_system(Vfs::Env &env,
                                           Xml_node  node)
:
	Internal_file_system_factory { env, node },
	Vfs::Dir_file_system         { env, Xml_node { _config(node).string() }, *this }
{ }


/**********************************
 ** Frontend_file_system_Factory **
 **********************************/

Vfs::File_system *Frontend_file_system_factory::create(Vfs::Env &env,
                                                       Xml_node  node)
{
	try {
		return new (env.alloc())
			Root_dir_file_system { env, node };

	} catch (...) {

		error("vfs_cbe_trust_anchor: failed to create file system");
	}
	return nullptr;
}


/**************************
 ** VFS plugin interface **
 **************************/

extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	static Frontend_file_system_factory factory { };
	return &factory;
}
