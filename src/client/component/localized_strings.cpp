#include <std_include.hpp>
#include <filesystem>
#include <fstream>

#include "loader/component_loader.hpp"
#include "localized_strings.hpp"
#include "utils/hook.hpp"
#include "utils/string.hpp"
#include "game/game.hpp"
#include "utils/json.hpp"

using json = nlohmann::json;

namespace localized_strings
{
	namespace
	{
		utils::hook::detour seh_string_ed_get_string_hook;

		std::unordered_map<std::string, std::string>& get_localized_overrides()
		{
			static std::unordered_map<std::string, std::string> overrides;
			return overrides;
		}

		std::mutex& get_synchronization_mutex()
		{
			static std::mutex mutex;
			return mutex;
		}

		const char* seh_string_ed_get_string(const char* reference)
		{
			std::lock_guard<std::mutex> _(get_synchronization_mutex());

			auto& overrides = get_localized_overrides();
			const auto entry = overrides.find(reference);
			if (entry != overrides.end())
			{
				return utils::string::va("%s", entry->second.data());
			}

			return seh_string_ed_get_string_hook.invoke<const char*>(reference);
		}

		unsigned int g_localized_string_index_stub(const char* name, /*ConfigString*/ unsigned int start,
		                                           unsigned int max, int create,
		                                           const char* errormsg)
		{
			create = 1;
			return game::G_FindConfigstringIndex(name, start, max, create, errormsg);
		}

		void replaceAll(std::string& str, const std::string& from, const std::string& to) {
			if (from.empty())
				return;
			size_t start_pos = 0;
			while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
				str.replace(start_pos, from.length(), to);
				start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
			}
		}

		game::XAssetEntry* __fastcall link_localized_string_stub(game::XAssetType type, game::XAssetHeader* header)
		{
			auto& overrides = get_localized_overrides();
			const auto entry = overrides.find(header->localize->name);
			if (entry != overrides.end())
			{
				header->localize->value = _strdup(entry->second.data());
			}

			return game::DB_LinkXAssetEntry1(type, header);
		}

		void load_chs()
		{
			if (std::filesystem::exists("iw6x\\chs.json"))
			{
				printf("Loading chs.json\n");

				std::ifstream file("iw6x\\chs.json");
				json chs;
				file >> chs;

				for (auto& [key, value] : chs.items())
				{
					localized_strings::override(key.data(), value.get<std::string>().data());
				}
			}	
		}
	}

	void override(const std::string& key, const std::string& value)
	{
		std::lock_guard<std::mutex> _(get_synchronization_mutex());
		get_localized_overrides()[key] = value;
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			// Change some localized strings
			// seh_string_ed_get_string_hook.create(SELECT_VALUE(0x1403F42D0, 0x1404A5F60), &seh_string_ed_get_string);

			if (!game::environment::is_sp())
			{
				load_chs();
				utils::hook::call(0x140325B4A, link_localized_string_stub);

				// Allocate localized strings after pre_main
				utils::hook::call(0x140163D0A, g_localized_string_index_stub);
			}
		}
	};
}

REGISTER_COMPONENT(localized_strings::component)
