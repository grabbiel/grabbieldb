#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sqlite3.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define ADMIN_PORT 8888
#define BUFFER_SIZE 16384
#define DB_PATH "/var/lib/grabbiel-db/content.db"

struct Column {
  std::string name;
  std::string type;
};

struct Table {
  std::string name;
  std::vector<Column> columns;
};

std::vector<std::string> get_tables(sqlite3 *db) {
  std::vector<std::string> tables;
  sqlite3_stmt *stmt;

  const char *sql =
      "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return tables;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *table_name = (const char *)sqlite3_column_text(stmt, 0);
    tables.push_back(table_name);
  }

  sqlite3_finalize(stmt);
  return tables;
}

// Get columns for a specific table
std::vector<Column> get_table_columns(sqlite3 *db,
                                      const std::string &table_name) {
  std::vector<Column> columns;
  sqlite3_stmt *stmt;

  std::string sql = "PRAGMA table_info(" + table_name + ");";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
    return columns;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Column col;
    col.name = (const char *)sqlite3_column_text(stmt, 1);
    col.type = (const char *)sqlite3_column_text(stmt, 2);
    columns.push_back(col);
  }

  sqlite3_finalize(stmt);
  return columns;
}

// Get all table records
std::vector<std::map<std::string, std::string>>
get_table_data(sqlite3 *db, const std::string &table_name,
               const std::vector<Column> &columns) {
  std::vector<std::map<std::string, std::string>> records;
  sqlite3_stmt *stmt;

  std::string sql = "SELECT * FROM " + table_name + ";";

  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
    return records;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::map<std::string, std::string> record;
    for (int i = 0; i < columns.size(); i++) {
      const char *value = (const char *)sqlite3_column_text(stmt, i);
      record[columns[i].name] = value ? value : "NULL";
    }
    records.push_back(record);
  }

  sqlite3_finalize(stmt);
  return records;
}

// Parse URL parameters
std::map<std::string, std::string> parse_params(const std::string &query) {
  std::map<std::string, std::string> params;
  std::istringstream iss(query);
  std::string pair;

  while (std::getline(iss, pair, '&')) {
    std::istringstream pair_stream(pair);
    std::string key, value;

    if (std::getline(pair_stream, key, '=')) {
      std::getline(pair_stream, value);
      params[key] = value;
    }
  }

  return params;
}

// Generate HTML for the main page
std::string generate_main_page(sqlite3 *db) {
  std::stringstream html;
  std::vector<std::string> tables = get_tables(db);

  html << "<!DOCTYPE html>"
       << "<html><head><title>SQLite Admin</title>"
       << "<style>"
       << "body { font-family: Arial, sans-serif; margin: 20px; }"
       << "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
       << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
       << "th { background-color: #f2f2f2; }"
       << "tr:hover { background-color: #f5f5f5; }"
       << ".menu { display: flex; background-color: #333; padding: 10px; }"
       << ".menu a { color: white; padding: 10px; text-decoration: none; }"
       << ".menu a:hover { background-color: #555; }"
       << "</style></head><body>"
       << "<h1>SQLite Database Admin</h1>"
       << "<div class='menu'>"
       << "<a href='/'>Tables</a>";

  for (const auto &table : tables) {
    html << "<a href='/table?name=" << table << "'>" << table << "</a>";
  }

  html << "</div><h2>Database Tables</h2><ul>";

  for (const auto &table : tables) {
    html << "<li><a href='/table?name=" << table << "'>" << table
         << "</a></li>";
  }

  html << "</ul></body></html>";

  return html.str();
}

// Generate HTML for table view
std::string generate_table_view(sqlite3 *db, const std::string &table_name) {
  std::stringstream html;
  std::vector<Column> columns = get_table_columns(db, table_name);
  std::vector<std::map<std::string, std::string>> records =
      get_table_data(db, table_name, columns);

  std::vector<std::string> tables = get_tables(db);

  html << "<!DOCTYPE html>"
       << "<html><head><title>Table: " << table_name << "</title>"
       << "<style>"
       << "body { font-family: Arial, sans-serif; margin: 20px; }"
       << "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
       << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
       << "th { background-color: #f2f2f2; }"
       << "tr:hover { background-color: #f5f5f5; }"
       << ".menu { display: flex; background-color: #333; padding: 10px; }"
       << ".menu a { color: white; padding: 10px; text-decoration: none; }"
       << ".menu a:hover { background-color: #555; }"
       << ".actions { display: flex; gap: 10px; margin-top: 20px; }"
       << "button { padding: 10px; background-color: #4CAF50; color: white; "
          "border: none; cursor: pointer; }"
       << "button:hover { background-color: #45a049; }"
       << "</style></head><body>"
       << "<h1>SQLite Database Admin</h1>"
       << "<div class='menu'>"
       << "<a href='/'>Tables</a>";

  for (const auto &table : tables) {
    html << "<a href='/table?name=" << table << "'>" << table << "</a>";
  }

  html << "</div>"
       << "<h2>Table: " << table_name << "</h2>"
       << "<div class='actions'>"
       << "<button onclick=\"location.href='/insert?table=" << table_name
       << "'\">Add New Record</button>"
       << "<button onclick=\"location.href='/export?table=" << table_name
       << "'\">Export CSV</button>"
       << "</div>"
       << "<table><tr>";

  // Table headers
  for (const auto &column : columns) {
    html << "<th>" << column.name << " (" << column.type << ")</th>";
  }
  html << "<th>Actions</th></tr>";

  // Table data
  for (const auto &record : records) {
    html << "<tr>";

    for (const auto &column : columns) {
      html << "<td>" << record.at(column.name) << "</td>";
    }

    // Add action buttons for each row
    html << "<td>"
         << "<a href='/edit?table=" << table_name << "&id=" << record.at("id")
         << "'>Edit</a> | "
         << "<a href='/delete?table=" << table_name << "&id=" << record.at("id")
         << "' onclick='return confirm(\"Are you sure?\")'>Delete</a>"
         << "</td>";

    html << "</tr>";
  }

  html << "</table></body></html>";

  return html.str();
}

void handle_request(int client_socket, const char *request) {
  sqlite3 *db;
  int rc = sqlite3_open(DB_PATH, &db);

  if (rc) {
    sqlite3_close(db);
    std::string response = "HTTP/1.1 500 Internal Server Error\r\n";
    response += "Content-Type: text/plain\r\n\r\n";
    response += "Failed to open database";
    send(client_socket, response.c_str(), response.length(), 0);
    return;
  }

  std::string req(request);
  std::string response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: text/html; charset=UTF-8\r\n\r\n";

  // Parse request path
  size_t path_start = req.find(" ") + 1;
  size_t path_end = req.find(" ", path_start);
  std::string path = req.substr(path_start, path_end - path_start);

  // Parse URL parameters
  std::map<std::string, std::string> params;
  size_t query_start = path.find('?');

  if (query_start != std::string::npos) {
    std::string query = path.substr(query_start + 1);
    path = path.substr(0, query_start);
    params = parse_params(query);
  }

  // Route requests
  if (path == "/" || path == "/index") {
    response += generate_main_page(db);
  } else if (path == "/table" && params.find("name") != params.end()) {
    response += generate_table_view(db, params["name"]);
  } else {
    response = "HTTP/1.1 404 Not Found\r\n";
    response += "Content-Type: text/plain\r\n\r\n";
    response += "404 - Page not found";
  }

  sqlite3_close(db);
  send(client_socket, response.c_str(), response.length(), 0);
}

int main() {
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  socklen_t addrlen = sizeof(address);
  char buffer[BUFFER_SIZE] = {0};

  // Create socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Set socket options
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
                 sizeof(opt))) {
    perror("Setsockopt failed");
    exit(EXIT_FAILURE);
  }

  // Configure address
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Only bind to localhost
  address.sin_port = htons(ADMIN_PORT);

  // Bind socket
  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, 3) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  printf("SQLite Admin Server started on localhost:%d\n", ADMIN_PORT);

  while (1) {
    if ((new_socket =
             accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }

    memset(buffer, 0, BUFFER_SIZE);
    int bytes_read = read(new_socket, buffer, BUFFER_SIZE);

    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      handle_request(new_socket, buffer);
    }

    close(new_socket);
  }

  close(server_fd);
  return 0;
}
