#include <KlayGE/KlayGE.hpp>
#include <KFL/Hash.hpp>
#include <KFL/Util.hpp>
#include <KlayGE/JudaTexture.hpp>
#include <KlayGE/ResLoader.hpp>
#include <KFL/XMLDom.hpp>
#include <KFL/CXX17/filesystem.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <regex>

#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable" // Ignore unused variable (mpl_assertion_in_line_xxx) in boost
#endif
#include <boost/algorithm/string/case_conv.hpp>
#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic pop
#endif

#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable" // Ignore unused variable (mpl_assertion_in_line_xxx) in boost
#endif
#include <boost/program_options.hpp>
#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic pop
#endif
#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable" // Ignore unused variable (mpl_assertion_in_line_xxx) in boost
#endif
#include <boost/algorithm/string/split.hpp>
#if defined(KLAYGE_COMPILER_CLANGC2)
#pragma clang diagnostic pop
#endif
#include <boost/algorithm/string/trim.hpp>

#include <KlayGE/ToolCommon.hpp>
#include <KlayGE/TexConverter.hpp>
#include <KlayGE/TexMetadata.hpp>

using namespace std;
using namespace KlayGE;

struct OfflineRenderDeviceCaps
{
	std::string platform;
	uint8_t major_version;
	uint8_t minor_version;

	bool bc1_support : 1;
	bool bc3_support : 1;
	bool bc4_support : 1;
	bool bc5_support : 1;
	bool bc7_support : 1;
	bool etc1_support : 1;
	bool r16_support : 1;
	bool r16f_support : 1;
	bool srgb_support : 1;
};

int RetrieveAttrValue(XMLNodePtr node, std::string const & attr_name, int default_value)
{
	XMLAttributePtr attr = node->Attrib(attr_name);
	if (attr)
	{
		return attr->ValueInt();
	}

	return default_value;
}

std::string RetrieveAttrValue(XMLNodePtr node, std::string const & attr_name, std::string const & default_value)
{
	XMLAttributePtr attr = node->Attrib(attr_name);
	if (attr)
	{
		return std::string(attr->ValueString());
	}

	return default_value;
}

int RetrieveNodeValue(XMLNodePtr root, std::string const & node_name, int default_value)
{
	XMLNodePtr node = root->FirstNode(node_name);
	if (node)
	{
		return RetrieveAttrValue(node, "value", default_value);
	}

	return default_value;
}

std::string RetrieveNodeValue(XMLNodePtr root, std::string const & node_name, std::string const & default_value)
{
	XMLNodePtr node = root->FirstNode(node_name);
	if (node)
	{
		return RetrieveAttrValue(node, "value", default_value);
	}

	return default_value;
}

OfflineRenderDeviceCaps LoadPlatformConfig(std::string const & platform)
{
	ResIdentifierPtr plat = ResLoader::Instance().Open("PlatConf/" + platform + ".plat");

	KlayGE::XMLDocument doc;
	XMLNodePtr root = doc.Parse(plat);

	OfflineRenderDeviceCaps caps;

	caps.platform = RetrieveAttrValue(root, "name", "");
	caps.major_version = static_cast<uint8_t>(RetrieveAttrValue(root, "major_version", 0));
	caps.minor_version = static_cast<uint8_t>(RetrieveAttrValue(root, "minor_version", 0));

	caps.bc1_support = RetrieveNodeValue(root, "bc1_support", 0) ? true : false;
	caps.bc3_support = RetrieveNodeValue(root, "bc3_support", 0) ? true : false;
	caps.bc4_support = RetrieveNodeValue(root, "bc4_support", 0) ? true : false;
	caps.bc5_support = RetrieveNodeValue(root, "bc5_support", 0) ? true : false;
	caps.bc7_support = RetrieveNodeValue(root, "bc7_support", 0) ? true : false;
	caps.etc1_support = RetrieveNodeValue(root, "etc1_support", 0) ? true : false;
	caps.r16_support = RetrieveNodeValue(root, "r16_support", 0) ? true : false;
	caps.r16f_support = RetrieveNodeValue(root, "r16f_support", 0) ? true : false;
	caps.srgb_support = RetrieveNodeValue(root, "srgb_support", 0) ? true : false;

	return caps;
}

TexMetadata DefaultTextureMetadata(size_t res_type_hash, OfflineRenderDeviceCaps const & caps)
{
	TexMetadata default_metadata;
	switch (res_type_hash)
	{
	case CT_HASH("albedo"):
	case CT_HASH("emissive"):
		default_metadata.Slot((res_type_hash == CT_HASH("albedo")) ? RenderMaterial::TS_Albedo : RenderMaterial::TS_Emissive);
		default_metadata.ForceSRGB(true);
		if (caps.bc7_support)
		{
			default_metadata.PreferedFormat(EF_BC7_SRGB);
		}
		else if (caps.bc1_support)
		{
			default_metadata.PreferedFormat(EF_BC1_SRGB);
		}
		else if (caps.etc1_support)
		{
			default_metadata.PreferedFormat(EF_ETC1);
		}
		break;

	case CT_HASH("glossiness"):
	case CT_HASH("metalness"):
		default_metadata.Slot((res_type_hash == CT_HASH("glossiness")) ? RenderMaterial::TS_Glossiness : RenderMaterial::TS_Metalness);
		default_metadata.ForceSRGB(false);
		if (caps.bc7_support)
		{
			default_metadata.PreferedFormat(EF_BC7);
		}
		else if (caps.bc1_support)
		{
			default_metadata.PreferedFormat(EF_BC1);
		}
		else if (caps.etc1_support)
		{
			default_metadata.PreferedFormat(EF_ETC1);
		}
		break;

	case CT_HASH("normal"):
	case CT_HASH("bump"):
		default_metadata.Slot(RenderMaterial::TS_Normal);
		default_metadata.ForceSRGB(false);
		if (res_type_hash == CT_HASH("bump"))
		{
			default_metadata.BumpToNormal(true);
			default_metadata.BumpScale(1.0f);
		}
		if (caps.bc5_support)
		{
			default_metadata.PreferedFormat(EF_BC5);
		}
		else if (caps.bc3_support)
		{
			default_metadata.PreferedFormat(EF_BC3);
		}
		else
		{
			default_metadata.PreferedFormat(EF_GR8);
		}
		break;

	case CT_HASH("height"):
		default_metadata.Slot(RenderMaterial::TS_Height);
		default_metadata.ForceSRGB(false);
		if (caps.bc4_support)
		{
			default_metadata.PreferedFormat(EF_BC4);
		}
		else if (caps.bc1_support)
		{
			default_metadata.PreferedFormat(EF_BC1);
		}
		else if (caps.etc1_support)
		{
			default_metadata.PreferedFormat(EF_ETC1);
		}
		break;
	}

	default_metadata.MipmapEnabled(true);
	default_metadata.AutoGenMipmap(true);

	return default_metadata;
}

TexMetadata LoadTextureMetadata(std::string const & res_name, TexMetadata const & default_metadata)
{
	std::string metadata_name = res_name + ".kmeta";
	if (ResLoader::Instance().Locate(metadata_name).empty())
	{
		return default_metadata;
	}
	else
	{
		return TexMetadata(metadata_name);
	}
}

void Deploy(std::vector<std::string> const & res_names, std::string const & res_type, OfflineRenderDeviceCaps const & caps)
{
	size_t const res_type_hash = RT_HASH(res_type.c_str());

	if ((CT_HASH("albedo") == res_type_hash)
		|| (CT_HASH("emissive") == res_type_hash)
		|| (CT_HASH("glossiness") == res_type_hash)
		|| (CT_HASH("metalness") == res_type_hash)
		|| (CT_HASH("normal") == res_type_hash)
		|| (CT_HASH("bump") == res_type_hash)
		|| (CT_HASH("height") == res_type_hash))
	{
		TexMetadata const default_metadata = DefaultTextureMetadata(res_type_hash, caps);

		TexConverter tc;
		for (size_t i = 0; i < res_names.size(); ++ i)
		{
			std::cout << "Converting " << res_names[i] << " to " << res_type << std::endl;

			TexMetadata metadata = LoadTextureMetadata(res_names[i], default_metadata);
			TexturePtr output_tex = tc.Convert(res_names[i], metadata);
			if (output_tex)
			{
				filesystem::path res_path(res_names[i]);
				std::string tex_base = (res_path.parent_path() / res_path.stem()).string();
				SaveTexture(output_tex, tex_base + ".dds");
			}
		}
	}
	else
	{
		std::ofstream ofs("convert.bat");

		if (CT_HASH("cubemap") == res_type_hash)
		{
			std::string y_fmt;
			std::string c_fmt;
			if (caps.r16_support)
			{
				y_fmt = "R16";
			}
			else if (caps.r16f_support)
			{
				y_fmt = "R16F";
			}
			if (caps.bc5_support)
			{
				c_fmt = "BC5";
			}
			else if (caps.bc3_support)
			{
				c_fmt = "BC3";
			}

			for (size_t i = 0; i < res_names.size(); ++ i)
			{
				std::cout << "Converting " << res_names[i] << " to " << res_type << std::endl;

				ofs << "@echo Processing: " << res_names[i] << std::endl;

				ofs << "@echo off" << std::endl << std::endl;
				ofs << "HDRCompressor \"" << res_names[i] << "\" " << y_fmt << ' ' << c_fmt << std::endl;
				ofs << "@echo on" << std::endl << std::endl;
			}
		}
		else if (CT_HASH("model") == res_type_hash)
		{
			for (size_t i = 0; i < res_names.size(); ++ i)
			{
				std::cout << "Converting " << res_names[i] << " to " << res_type << std::endl;

				ofs << "@echo Processing: " << res_names[i] << std::endl;

				ofs << "@echo off" << std::endl << std::endl;
				ofs << "MeshMLJIT -I \"" << res_names[i] << "\" -P " << caps.platform << std::endl;
				ofs << "@echo on" << std::endl << std::endl;
			}
		}
		else if (CT_HASH("effect") == res_type_hash)
		{
			for (size_t i = 0; i < res_names.size(); ++ i)
			{
				std::cout << "Converting " << res_names[i] << " to " << res_type << std::endl;

				ofs << "@echo Processing: " << res_names[i] << std::endl;

				ofs << "@echo off" << std::endl << std::endl;
				ofs << "FXMLJIT " << caps.platform << " \"" << res_names[i] << "\"" << std::endl;
				ofs << "@echo on" << std::endl << std::endl;
			}
		}
		else
		{
			std::cout << "Error: Unknown resource type." << std::endl;
		}

		ofs.close();

		if ((res_type_hash != CT_HASH("cubemap")) && (res_type_hash != CT_HASH("model")) && (res_type_hash != CT_HASH("effect")))
		{
			int err = system("convert.bat");
			KFL_UNUSED(err);
		}

		int err = system("del convert.bat");
		KFL_UNUSED(err);
	}
}

int main(int argc, char* argv[])
{
	ResLoader::Instance().AddPath("../../Tools/media/PlatformDeployer");

	std::vector<std::string> res_names;
	std::string res_type;
	std::string platform;

	boost::program_options::options_description desc("Allowed options");
	desc.add_options()
		("help,H", "Produce help message")
		("input-name,I", boost::program_options::value<std::string>(), "Input resource name.")
		("type,T", boost::program_options::value<std::string>(), "Resource type.")
		("platform,P", boost::program_options::value<std::string>(), "Platform name.")
		("version,v", "Version.");

	boost::program_options::variables_map vm;
	boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
	boost::program_options::notify(vm);

	if ((argc <= 1) || (vm.count("help") > 0))
	{
		cout << desc << endl;
		Context::Destroy();
		return 1;
	}
	if (vm.count("version") > 0)
	{
		cout << "KlayGE PlatformDeployer, Version 2.0.0" << endl;
		Context::Destroy();
		return 1;
	}
	if (vm.count("input-name") > 0)
	{
		std::string input_name_str = vm["input-name"].as<std::string>();

		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, input_name_str, boost::is_any_of(",;"));
		for (auto& arg : tokens)
		{
			boost::algorithm::trim(arg);
			if ((std::string::npos == arg.find('*')) && (std::string::npos == arg.find('?')))
			{
				res_names.push_back(arg);
			}
			else
			{
				std::regex const filter(DosWildcardToRegex(arg));

				filesystem::directory_iterator end_itr;
				for (filesystem::directory_iterator i("."); i != end_itr; ++ i)
				{
					if (filesystem::is_regular_file(i->status()))
					{
						std::smatch what;
						std::string const name = i->path().filename().string();
						if (std::regex_match(name, what, filter))
						{
							res_names.push_back(name);
						}
					}
				}
			}
		}
	}
	else
	{
		cout << "Need input resources names." << endl;
		Context::Destroy();
		return 1;
	}
	if (vm.count("type") > 0)
	{
		res_type = vm["type"].as<std::string>();
	}
	else
	{
		std::string ext_name = filesystem::path(res_names[0]).extension().string();
		if (".dds" == ext_name)
		{
			res_type = "albedo";
		}
		else if (".meshml" == ext_name)
		{
			res_type = "model";
		}
		else
		{
			cout << "Need resource type name." << endl;
			Context::Destroy();
			return 1;
		}
	}
	if (vm.count("platform") > 0)
	{
		platform = vm["platform"].as<std::string>();
	}
	else
	{
		platform = "d3d_11_0";
	}

	boost::algorithm::to_lower(res_type);
	boost::algorithm::to_lower(platform);

	if (("pc_dx11" == platform) || ("pc_dx10" == platform) || ("pc_dx9" == platform) || ("win_tegra3" == platform)
		|| ("pc_gl4" == platform) || ("pc_gl3" == platform) || ("pc_gl2" == platform)
		|| ("android_tegra3" == platform) || ("ios" == platform))
	{
		if ("pc_dx11" == platform)
		{
			platform = "d3d_11_0";
		}
		else if ("pc_dx10" == platform)
		{
			platform = "d3d_10_0";
		}
		else if ("pc_dx9" == platform)
		{
			platform = "d3d_9_3";
		}
		else if ("win_tegra3" == platform)
		{
			platform = "d3d_9_1";
		}
		else if ("pc_gl4" == platform)
		{
			platform = "gl_4_0";
		}
		else if ("pc_gl3" == platform)
		{
			platform = "gl_3_0";
		}
		else if ("pc_gl2" == platform)
		{
			platform = "gl_2_0";
		}
		else if ("android_tegra3" == platform)
		{
			platform = "gles_2_0";
		}
		else if ("ios" == platform)
		{
			platform = "gles_2_0";
		}
	}

	OfflineRenderDeviceCaps caps = LoadPlatformConfig(platform);
	Deploy(res_names, res_type, caps);

	Context::Destroy();

	return 0;
}
