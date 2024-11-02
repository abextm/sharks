// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "config.hxx"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <iostream>
#include <utility>

Config *config;

static const char *DEFAULT_CONFIG = R"(# default sharks config

# On wayland you can use `pkill sharks -q 1397445443 -SIGUSR1` to trigger a screenshot
# and `pkill sharks -q 1397444683 -SIGUSR1` to trigger the picker
[globalkeys]
screenshot = ["Ctrl+Print"]
picker = ["Alt+Print"]

[pen]
key = "P"
color = 0xFF0000
thickness = 4

[action.copy]
name = "Copy"
action = "copy"
icon = "edit-copy"
key = "Ctrl+C"
enabled = true

[action.save]
name = "Save"
action = "save-default"
icon = "document-save"
enabled = true

[action.save-as]
name = "Save-as"
action = "save-as"
icon = "document-save-as"
key = "Ctrl+S"
enabled = true

[action.upload]
name = "Upload"
action = "exec"
args = ["ymup", "-if"]
icon = "document-send"
enabled = false
)";

void Config::init() {
	const char *CONFIG_FILE_NAME = "sharks.toml";

	QString path = QStandardPaths::locate(QStandardPaths::AppConfigLocation, CONFIG_FILE_NAME);
	if (path.isEmpty()) {
		auto dir = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
		if (!dir.exists()) {
			dir.mkpath(".");
		}
		path = dir.filePath(CONFIG_FILE_NAME);
		qInfo() << "Making default config at" << path;

		QFile fi(path);
		fi.open(QFile::WriteOnly | QFile::NewOnly);
		fi.write(DEFAULT_CONFIG);
	}

	toml::table root;
	try {
		root = toml::parse_file(path.toStdString());
	} catch (toml::parse_error &e) {
		std::cerr << "Failed to parse config" << e << std::endl;
	}

	config = new Config(root);
}

static void mergeInto(toml::table *into, toml::table *from) {
	for (auto &entry : *from) {
		if (into->get(entry.first) == nullptr) {
			into->insert_or_assign(entry.first, entry.second);
		} else if (entry.second.is_table()) {
			auto *intoTab = into->get_as<toml::table>(entry.first);
			if (intoTab == nullptr) {
				into->insert_or_assign(entry.first, entry.second);
			} else {
				mergeInto(intoTab, entry.second.as_table());
			}
		}
	}
}

Config::Config(toml::table root)
	: defaultRoot(toml::parse(DEFAULT_CONFIG)),
		root(std::move(root)) {
	mergeInto(&this->root, &this->defaultRoot);
}

QList<QKeySequence> Config::parseHotkeys(toml::node_view<toml::node> &&node) {
	if (node.is_string()) {
		auto str = node.as_string()->get();
		return QKeySequence::listFromString(QString::fromStdString(str));
	} else if (node.is_array()) {
		QList<QKeySequence> l;
		for (auto &entry : *node.as_array()) {
			if (entry.is_string()) {
				auto str = entry.as_string()->get();
				l.append(QKeySequence::fromString(QString::fromStdString(str)));
			} else {
				Config::complain(toml::node_view(entry), "wanted a string");
			}
		}
	} else if (node) {
		Config::complain(node, "wanted a string or array of strings");
	}

	QList<QKeySequence> l;
	return l;
}

void Config::complain(const toml::node *node, const QString &message) {
	if (node) {
		auto &source = node->source();
		qInfo() << source.path.get() << ":" << source.begin.line << ":" << message;
	}
}

void Config::complain(const toml::node_view<toml::node> node, const QString &message) {
	Config::complain(node.node(), message);
}
