// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

#ifndef SHARKS_CONFIG_HXX
#define SHARKS_CONFIG_HXX

#include <QKeySequence>
#include <toml++/toml.hpp>

class Config {
 private:
	explicit Config(toml::table);

 public:
	static void init();
	static QList<QKeySequence> parseHotkeys(toml::node_view<toml::node> &&node);
	static void complain(const toml::node *node, const QString &message);
	static void complain(toml::node_view<toml::node> node, const QString &message);

	template <class T>
	static std::optional<T> get(const toml::table *node, std::string_view key, QString complain, QString complainNull = "") {
		auto *v = node->get(key);
		if (v == nullptr) {
			if (!complainNull.isEmpty()) {
				Config::complain(node, complainNull);
			}
			return {};
		}

		auto *tv = v->as<T>();
		if (!tv) {
			Config::complain(v, complain);
			return {};
		}

		if constexpr (std::is_same_v<std::remove_cv_t<std::remove_reference_t<decltype(*tv)>>, T>) {
			return std::make_optional(*tv);
		} else {
			return std::make_optional(**tv);
		}
	}

	toml::table defaultRoot;
	toml::table root;
};

extern Config *config;

#endif	// SHARKS_CONFIG_HXX
