#include "core/database/src/chatdb_init.h"
#include <pqxx/pqxx>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace DBInit {

static bool should_skip_line(const std::string &line) {
	std::string trimmed = line;
	// trim leading spaces
	trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char c){return !std::isspace(c);}));
	if (trimmed.rfind("\\c ", 0) == 0) return true; // psql connect meta-command
	if (trimmed.rfind("CREATE USER ", 0) == 0) return true;
	if (trimmed.rfind("DROP DATABASE ", 0) == 0) return true;
	if (trimmed.rfind("CREATE DATABASE ", 0) == 0) return true;
	return false;
}

bool initializeSchema(const std::string& conninfo, const std::string& sqlFilePath) {
	try {
		pqxx::connection conn(conninfo);
		if (!conn.is_open()) {
			std::cerr << "Failed to open database connection\n";
			return false;
		}

		std::ifstream sqlFile(sqlFilePath);
		if (!sqlFile.is_open()) {
			std::cerr << "Failed to open SQL file: " << sqlFilePath << "\n";
			return false;
		}

		std::stringstream filtered;
		std::string line;
		while (std::getline(sqlFile, line)) {
			if (should_skip_line(line)) continue;
			filtered << line << '\n';
		}
		const std::string sql = filtered.str();

		pqxx::work txn(conn);
		txn.exec(sql);
		txn.commit();

		std::cout << "Database schema initialized successfully.\n";
		return true;
	}
	catch (const std::exception& e) {
		std::cerr << "Error initializing database schema: " << e.what() << "\n";
		return false;
	}
}

} // namespace DBInit

int main(int argc, char** argv) {
	std::string conninfo;
	std::string sqlPath;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		const std::string connPrefix = "--conninfo=";
		const std::string sqlPrefix = "--sql=";
		if (arg.rfind(connPrefix, 0) == 0) {
			conninfo = arg.substr(connPrefix.size());
		} else if (arg.rfind(sqlPrefix, 0) == 0) {
			sqlPath = arg.substr(sqlPrefix.size());
		}
	}
	if (conninfo.empty() || sqlPath.empty()) {
		std::cerr << "Usage: chatdb_init --conninfo=... --sql=/absolute/path/to/schema.sql\n";
		return 2;
	}
	return DBInit::initializeSchema(conninfo, sqlPath) ? 0 : 1;
}
