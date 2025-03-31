#include <arpa/inet.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#define MEDIA_PORT 8889
#define BUFFER_SIZE 65536
#define DB_PATH "/var/lib/grabbiel-db/content.db"
#define TEMP_UPLOAD_DIR "/tmp/grabbiel-uploads"

struct Image {
  int id;
  std::string original_url;
  std::string filename;
  std::string mime_type;
  int size;
  int width;
  int height;
  int content_id;
  std::string image_type;
  std::string processing_status;
};

struct Video {
  int id;
  std::string title;
  std::string gcs_path;
  std::string mime_type;
  int size_bytes;
  int duration_seconds;
  int content_id;
  std::string processing_status;
};

void log_to_file(const std::string &message) {
  std::ofstream log_file("/tmp/grabbiel-debug.log", std::ios::app);
  if (log_file) {
    log_file << "[" << time(NULL) << "] " << message << std::endl;
    log_file.close();
  }
}

void log_file_content(const std::string &filepath, size_t max_bytes = 100) {
  std::ifstream file(filepath, std::ios::binary);
  if (!file) {
    log_to_file("Failed to open file for debugging: " + filepath);
    return;
  }

  std::vector<char> buffer(max_bytes);
  file.read(buffer.data(), max_bytes);
  size_t bytes_read = file.gcount();

  std::stringstream hex_dump;
  for (size_t i = 0; i < bytes_read; i++) {
    hex_dump << std::hex << std::setw(2) << std::setfill('0')
             << (int)(unsigned char)buffer[i] << " ";
    if ((i + 1) % 16 == 0)
      hex_dump << "\n";
  }

  log_to_file("File preview for " + filepath + " (" +
              std::to_string(bytes_read) + " bytes):\n" + hex_dump.str());
}

// Utility function to parse multipart form data
std::map<std::string, std::string>
parse_multipart_form_data(const std::string &body, const std::string &boundary,
                          std::map<std::string, std::vector<char>> &files) {
  std::map<std::string, std::string> form_data;
  std::string delimiter = "--" + boundary;

  log_to_file("Parsing multipart form data with boundary: " + boundary);
  log_to_file("Total body size: " + std::to_string(body.size()) + " bytes");

  size_t pos = 0;
  size_t next_pos = body.find(delimiter, pos);

  while (next_pos != std::string::npos) {
    pos = next_pos + delimiter.length();

    // Check if this is the last boundary marker
    if (body.substr(pos, 2) == "--") {
      log_to_file("Found final boundary marker");
      break;
    }

    // Skip the CRLF after the boundary
    if (body.substr(pos, 2) == "\r\n") {
      pos += 2;
    }

    // Find the end of this part
    next_pos = body.find(delimiter, pos);
    if (next_pos == std::string::npos) {
      log_to_file(
          "Warning: Could not find next boundary, data may be truncated");
      break;
    }

    // Extract the part content (including headers)
    std::string part = body.substr(pos, next_pos - pos);

    // Find the end of the headers
    size_t header_end = part.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      log_to_file("Warning: Malformed part, no header separator found");
      continue;
    }

    std::string headers = part.substr(0, header_end);
    // The content starts after the \r\n\r\n and ends with \r\n before the next
    // boundary
    std::string content;
    if (part.size() > header_end + 4) {
      // Check if the content ends with \r\n
      if (part.size() >= 2 && part.substr(part.size() - 2) == "\r\n") {
        content =
            part.substr(header_end + 4, part.size() - (header_end + 4) - 2);
      } else {
        content = part.substr(header_end + 4);
      }
    }

    // Extract field name and filename if present
    std::string name;
    std::string filename;

    size_t name_pos = headers.find("name=\"");
    if (name_pos != std::string::npos) {
      size_t name_end = headers.find("\"", name_pos + 6);
      name = headers.substr(name_pos + 6, name_end - (name_pos + 6));
    }

    size_t filename_pos = headers.find("filename=\"");
    if (filename_pos != std::string::npos) {
      size_t filename_end = headers.find("\"", filename_pos + 10);
      filename =
          headers.substr(filename_pos + 10, filename_end - (filename_pos + 10));
    }

    log_to_file("Found part name='" + name + "'" +
                (filename.empty() ? "" : ", filename='" + filename + "'"));

    // If we have a filename, this is a file upload
    if (!filename.empty()) {
      // Store file data
      std::vector<char> file_data(content.begin(), content.end());
      files[name] = file_data;

      log_to_file("Extracted file '" + filename +
                  "', size: " + std::to_string(file_data.size()) + " bytes");

      // Store the filename
      form_data[name + "_filename"] = filename;
    } else {
      form_data[name] = content;
      log_to_file("Extracted form field '" + name + "', value: '" + content +
                  "'");
    }
  }

  log_to_file("Finished parsing multipart form data, found " +
              std::to_string(files.size()) + " files and " +
              std::to_string(form_data.size() - files.size()) + " form fields");

  return form_data;
}

// Fetch images from database
std::vector<Image> get_images(sqlite3 *db, int limit = 20) {
  std::vector<Image> images;
  sqlite3_stmt *stmt;

  const char *sql = "SELECT id, original_url, filename, mime_type, size, "
                    "width, height, content_id, image_type, processing_status "
                    "FROM images ORDER BY id DESC LIMIT ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return images;
  }

  sqlite3_bind_int(stmt, 1, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Image img;
    img.id = sqlite3_column_int(stmt, 0);

    const char *url = (const char *)sqlite3_column_text(stmt, 1);
    img.original_url = url ? url : "";

    const char *filename = (const char *)sqlite3_column_text(stmt, 2);
    img.filename = filename ? filename : "";

    const char *mime = (const char *)sqlite3_column_text(stmt, 3);
    img.mime_type = mime ? mime : "";

    img.size = sqlite3_column_int(stmt, 4);
    img.width = sqlite3_column_int(stmt, 5);
    img.height = sqlite3_column_int(stmt, 6);
    img.content_id = sqlite3_column_int(stmt, 7);

    const char *type = (const char *)sqlite3_column_text(stmt, 8);
    img.image_type = type ? type : "";

    const char *status = (const char *)sqlite3_column_text(stmt, 9);
    img.processing_status = status ? status : "";

    images.push_back(img);
  }

  sqlite3_finalize(stmt);
  return images;
}

// Fetch videos from database
std::vector<Video> get_videos(sqlite3 *db, int limit = 20) {
  std::vector<Video> videos;
  sqlite3_stmt *stmt;

  const char *sql =
      "SELECT id, title, gcs_path, mime_type, size_bytes, duration_seconds, "
      "content_id, processing_status FROM videos ORDER BY id DESC LIMIT ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return videos;
  }

  sqlite3_bind_int(stmt, 1, limit);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Video vid;
    vid.id = sqlite3_column_int(stmt, 0);

    const char *title = (const char *)sqlite3_column_text(stmt, 1);
    vid.title = title ? title : "";

    const char *path = (const char *)sqlite3_column_text(stmt, 2);
    vid.gcs_path = path ? path : "";

    const char *mime = (const char *)sqlite3_column_text(stmt, 3);
    vid.mime_type = mime ? mime : "";

    vid.size_bytes = sqlite3_column_int(stmt, 4);
    vid.duration_seconds = sqlite3_column_int(stmt, 5);
    vid.content_id = sqlite3_column_int(stmt, 6);

    const char *status = (const char *)sqlite3_column_text(stmt, 7);
    vid.processing_status = status ? status : "";

    videos.push_back(vid);
  }

  sqlite3_finalize(stmt);
  return videos;
}

bool save_file(const std::vector<char> &file_data,
               const std::string &filename) {
  // Create upload directory if it doesn't exist
  system(("mkdir -p " + std::string(TEMP_UPLOAD_DIR)).c_str());

  std::string filepath = std::string(TEMP_UPLOAD_DIR) + "/" + filename;
  std::ofstream outfile(filepath, std::ios::binary);

  if (!outfile) {
    return false;
  }

  outfile.write(file_data.data(), file_data.size());
  outfile.close();

  return true;
}

// Execute a shell command and get output
std::string exec_command(const std::string &cmd) {
  std::string result;
  char buffer[128];
  FILE *pipe = popen(cmd.c_str(), "r");

  if (!pipe) {
    return "Error executing command";
  }

  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }

  pclose(pipe);
  return result;
}

// Upload a file to GCS
bool upload_to_gcs(const std::string &local_path, const std::string &gcs_path,
                   bool public_access = false) {
  std::string cmd = "sudo gsutil cp " + local_path + " " + gcs_path;
  std::string result = exec_command(cmd);

  if (public_access) {
    cmd = "sudo gsutil acl ch -u AllUsers:R " + gcs_path;
    exec_command(cmd);
  }

  // Check if upload was successful
  cmd = "sudo gsutil stat " + gcs_path + " 2>/dev/null";
  result = exec_command(cmd);

  return !result.empty();
}

// Insert image record into database
int insert_image(sqlite3 *db, const std::string &gcs_path,
                 const std::string &filename, const std::string &mime_type,
                 int size, int width, int height, int content_id,
                 const std::string &image_type) {
  sqlite3_stmt *stmt;
  const char *sql =
      "INSERT INTO images (original_url, filename, mime_type, size, width, "
      "height, content_id, image_type, processing_status) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'complete')";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return 0;
  }

  sqlite3_bind_text(stmt, 1, gcs_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, mime_type.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, size);
  sqlite3_bind_int(stmt, 5, width);
  sqlite3_bind_int(stmt, 6, height);
  sqlite3_bind_int(stmt, 7, content_id);
  sqlite3_bind_text(stmt, 8, image_type.c_str(), -1, SQLITE_STATIC);

  sqlite3_step(stmt);
  int last_id = sqlite3_last_insert_rowid(db);

  sqlite3_finalize(stmt);
  return last_id;
}

// Insert video record into database
int insert_video(sqlite3 *db, const std::string &title,
                 const std::string &gcs_path, const std::string &mime_type,
                 int size, int duration, int content_id) {
  sqlite3_stmt *stmt;
  const char *sql =
      "INSERT INTO videos (title, gcs_path, mime_type, size_bytes, "
      "duration_seconds, content_id, processing_status) "
      "VALUES (?, ?, ?, ?, ?, ?, 'complete')";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return 0;
  }

  sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 2, gcs_path.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt, 3, mime_type.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt, 4, size);
  sqlite3_bind_int(stmt, 5, duration);
  sqlite3_bind_int(stmt, 6, content_id);

  sqlite3_step(stmt);
  int last_id = sqlite3_last_insert_rowid(db);

  sqlite3_finalize(stmt);
  return last_id;
}

// Generate HTML for the main media manager page
std::string generate_main_page(sqlite3 *db) {
  std::vector<Image> images = get_images(db, 10);
  std::vector<Video> videos = get_videos(db, 10);

  std::stringstream html;
  html << "<!DOCTYPE html>" << "<html><head><title>Media Manager</title>"
       << "<style>" << "body { font-family: Arial, sans-serif; margin: 20px; }"
       << "h1, h2 { color: #333; }"
       << ".tabs { display: flex; margin-bottom: 20px; }"
       << ".tab { padding: 10px 20px; background: #f0f0f0; cursor: pointer; "
          "border: 1px solid #ccc; }"
       << ".tab.active { background: #007bff; color: white; border-color: "
          "#007bff; }"
       << ".tab-content { display: none; }"
       << ".tab-content.active { display: block; }"
       << ".media-grid { display: grid; grid-template-columns: "
          "repeat(auto-fill, minmax(200px, 1fr)); gap: 20px; }"
       << ".media-item { border: 1px solid #ddd; padding: 10px; border-radius: "
          "4px; }"
       << ".media-item img { width: 100%; height: 150px; object-fit: cover; }"
       << ".media-item .title { font-weight: bold; margin-top: 10px; }"
       << ".media-item .info { color: #666; font-size: 0.8em; }"
       << ".upload-form { margin: 20px 0; padding: 20px; border: 1px solid "
          "#ddd; border-radius: 4px; }"
       << "input, select, button { margin: 10px 0; padding: 8px; width: 100%; }"
       << ".button { background: #007bff; color: white; border: none; padding: "
          "10px 15px; cursor: pointer; }"
       << ".button:hover { background: #0069d9; }" << "</style>" << "<script>"
       << "function showTab(tabId) {"
       << "  document.querySelectorAll('.tab-content').forEach(tab => "
          "tab.classList.remove('active'));"
       << "  document.querySelectorAll('.tab').forEach(tab => "
          "tab.classList.remove('active'));"
       << "  document.getElementById(tabId).classList.add('active');"
       << "  "
          "document.querySelector(`[data-tab=\"${tabId}\"]`).classList.add('"
          "active');"
       << "}" << "</script>" << "</head><body>" << "<h1>Media Manager</h1>"
       << "<div class='tabs'>"
       << "<div class='tab active' data-tab='dashboard' "
          "onclick='showTab(\"dashboard\")'>Dashboard</div>"
       << "<div class='tab' data-tab='upload-image' "
          "onclick='showTab(\"upload-image\")'>Upload Image</div>"
       << "<div class='tab' data-tab='upload-video' "
          "onclick='showTab(\"upload-video\")'>Upload Video</div>"
       << "<div class='tab' data-tab='manage-images' "
          "onclick='showTab(\"manage-images\")'>Manage Images</div>"
       << "<div class='tab' data-tab='manage-videos' "
          "onclick='showTab(\"manage-videos\")'>Manage Videos</div>"
       << "</div>"

       << "<div id='dashboard' class='tab-content active'>"
       << "<h2>Media Dashboard</h2>" << "<div class='stats'>"
       << "<p>Recent Images: " << images.size() << "</p>"
       << "<p>Recent Videos: " << videos.size() << "</p>" << "</div>"
       << "<h3>Recent Images</h3>" << "<div class='media-grid'>";

  for (const auto &img : images) {
    html << "<div class='media-item'>" << "<img src='" << img.original_url
         << "' alt='" << img.filename << "'>" << "<div class='title'>"
         << img.filename << "</div>" << "<div class='info'>" << img.width << "x"
         << img.height << " | " << (img.size / 1024) << " KB</div>" << "</div>";
  }

  html << "</div>" << "<h3>Recent Videos</h3>" << "<div class='media-grid'>";

  for (const auto &vid : videos) {
    html << "<div class='media-item'>"
         << "<div class='video-placeholder' style='height: 150px; background: "
            "#eee; display: flex; align-items: center; justify-content: "
            "center;'>"
         << "<div style='font-size: 40px;'>▶️</div>" << "</div>"
         << "<div class='title'>" << vid.title << "</div>"
         << "<div class='info'>" << (vid.size_bytes / 1024 / 1024) << " MB | "
         << (vid.duration_seconds / 60) << ":" << (vid.duration_seconds % 60)
         << "</div>" << "</div>";
  }

  html
      << "</div>" << "</div>"

      << "<div id='upload-image' class='tab-content'>"
      << "<h2>Upload Image</h2>" << "<div class='upload-form'>"
      << "<form action='/upload-image' method='post' "
         "enctype='multipart/form-data'>"
      << "<div><label>Image File:</label><input type='file' name='image' "
         "accept='image/*' required></div>"
      << "<div><label>Associated Content ID:</label><input type='number' "
         "name='content_id' value='0'></div>"
      << "<div><label>Image Type:</label>" << "<select name='image_type'>"
      << "<option value='thumbnail'>Thumbnail</option>"
      << "<option value='content' selected>Content</option>"
      << "</select></div>" << "<div><label>Storage Type:</label>"
      << "<select name='storage_type'>"
      << "<option value='public' selected>Public</option>"
      << "<option value='private'>Private</option>" << "</select></div>"
      << "<div><button type='submit' class='button'>Upload Image</button></div>"
      << "</form>" << "</div>" << "</div>"

      << "<div id='upload-video' class='tab-content'>"
      << "<h2>Upload Video</h2>" << "<div class='upload-form'>"
      << "<form action='/upload-video' method='post' "
         "enctype='multipart/form-data'>"
      << "<div><label>Video File:</label><input type='file' name='video' "
         "accept='video/*' required></div>"
      << "<div><label>Title:</label><input type='text' name='title' "
         "required></div>"
      << "<div><label>Associated Content ID:</label><input type='number' "
         "name='content_id' value='0'></div>"
      << "<div><label>Duration (seconds):</label><input type='number' "
         "name='duration' value='0'></div>"
      << "<div><label>Storage Type:</label>" << "<select name='storage_type'>"
      << "<option value='public' selected>Public</option>"
      << "<option value='private'>Private</option>" << "</select></div>"
      << "<div><button type='submit' class='button'>Upload Video</button></div>"
      << "</form>" << "</div>" << "</div>"

      << "<div id='manage-images' class='tab-content'>"
      << "<h2>Manage Images</h2>" << "<div class='media-grid'>";

  for (const auto &img : images) {
    html << "<div class='media-item'>" << "<img src='" << img.original_url
         << "' alt='" << img.filename << "'>" << "<div class='title'>"
         << img.filename << "</div>" << "<div class='info'>" << img.width << "x"
         << img.height << " | " << (img.size / 1024) << " KB</div>"
         << "<div class='actions'>" << "<a href='/delete-image?id=" << img.id
         << "' onclick='return confirm(\"Are you sure you want to delete this "
            "image?\")'>Delete</a>"
         << "</div>" << "</div>";
  }

  html << "</div>" << "</div>"

       << "<div id='manage-videos' class='tab-content'>"
       << "<h2>Manage Videos</h2>" << "<div class='media-grid'>";

  for (const auto &vid : videos) {
    html << "<div class='media-item'>"
         << "<div class='video-placeholder' style='height: 150px; background: "
            "#eee; display: flex; align-items: center; justify-content: "
            "center;'>"
         << "<div style='font-size: 40px;'>▶️</div>" << "</div>"
         << "<div class='title'>" << vid.title << "</div>"
         << "<div class='info'>" << (vid.size_bytes / 1024 / 1024) << " MB | "
         << (vid.duration_seconds / 60) << ":" << (vid.duration_seconds % 60)
         << "</div>" << "<div class='actions'>"
         << "<a href='/delete-video?id=" << vid.id
         << "' onclick='return confirm(\"Are you sure you want to delete this "
            "video?\")'>Delete</a>"
         << "</div>" << "</div>";
  }

  html << "</div>" << "</div>"

       << "</body></html>";

  return html.str();
}

// Process image upload
std::string
handle_image_upload(sqlite3 *db,
                    const std::map<std::string, std::string> &form_data,
                    const std::map<std::string, std::vector<char>> &files) {
  std::stringstream response;
  response << "HTTP/1.1 303 See Other\r\n";
  response << "Location: /\r\n\r\n";

  if (files.find("image") == files.end()) {
    return response.str();
  }

  // Get form data
  const std::vector<char> &file_data = files.at("image");
  std::string filename = form_data.at("image_filename");

  log_to_file("Handling image upload: " + filename +
              ", size: " + std::to_string(file_data.size()) + " bytes");

  std::string image_type = form_data.find("image_type") != form_data.end()
                               ? form_data.at("image_type")
                               : "content";
  std::string storage_type = form_data.find("storage_type") != form_data.end()
                                 ? form_data.at("storage_type")
                                 : "public";
  int content_id = form_data.find("content_id") != form_data.end()
                       ? std::stoi(form_data.at("content_id"))
                       : 0;

  // Save file temporarily
  if (!save_file(file_data, filename)) {
    return response.str();
  }

  // Determine correct bucket and path
  std::string bucket = storage_type == "public" ? "gs://grabbiel-media-public"
                                                : "gs://grabbiel-media";
  std::string gcs_path = bucket + "/images/originals/" + filename;
  std::string local_path = std::string(TEMP_UPLOAD_DIR) + "/" + filename;

  log_to_file("File saved to: " + local_path);
  log_file_content(local_path);

  // Upload to GCS
  log_to_file("Attempting to upload to GCS: " + gcs_path);
  bool success = upload_to_gcs(local_path, gcs_path, storage_type == "public");
  log_to_file(std::string("GCS upload result: ") +
              (success ? "success" : "failure"));
  if (success) {
    // Get image dimensions (would require image processing library)
    // For now, we'll use placeholder values
    int width = 1920;
    int height = 1080;
    int size = file_data.size();

    // Determine the public URL
    std::string public_url;
    if (storage_type == "public") {
      public_url = "https://storage.googleapis.com/grabbiel-media-public/"
                   "images/originals/" +
                   filename;
    } else {
      public_url = gcs_path; // Store GCS path for private images
    }

    // Store in database
    insert_image(db, public_url, filename, "image/jpeg", size, width, height,
                 content_id, image_type);
  }

  // Clean up temporary file
  std::string rm_cmd = "rm -f " + local_path;
  system(rm_cmd.c_str());

  return response.str();
}

// Process video upload
std::string
handle_video_upload(sqlite3 *db,
                    const std::map<std::string, std::string> &form_data,
                    const std::map<std::string, std::vector<char>> &files) {
  std::stringstream response;
  response << "HTTP/1.1 303 See Other\r\n";
  response << "Location: /\r\n\r\n";

  if (files.find("video") == files.end()) {
    return response.str();
  }

  // Get form data
  const std::vector<char> &file_data = files.at("video");
  std::string filename = form_data.at("video_filename");
  std::string title = form_data.find("title") != form_data.end()
                          ? form_data.at("title")
                          : filename;
  std::string storage_type = form_data.find("storage_type") != form_data.end()
                                 ? form_data.at("storage_type")
                                 : "public";
  int content_id = form_data.find("content_id") != form_data.end()
                       ? std::stoi(form_data.at("content_id"))
                       : 0;
  int duration = form_data.find("duration") != form_data.end()
                     ? std::stoi(form_data.at("duration"))
                     : 0;

  // Save file temporarily
  if (!save_file(file_data, filename)) {
    return response.str();
  }

  // Determine correct bucket and path
  std::string bucket = storage_type == "public" ? "gs://grabbiel-media-public"
                                                : "gs://grabbiel-media";
  std::string gcs_path = bucket + "/videos/originals/" + filename;
  std::string local_path = std::string(TEMP_UPLOAD_DIR) + "/" + filename;

  // Upload to GCS
  bool success = upload_to_gcs(local_path, gcs_path, storage_type == "public");

  if (success) {
    // Store in database
    int size = file_data.size();
    insert_video(db, title, gcs_path, "video/mp4", size, duration, content_id);
  }

  // Clean up temporary file
  std::string rm_cmd = "rm -f " + local_path;
  system(rm_cmd.c_str());

  return response.str();
}

// Handle delete image request
std::string
handle_delete_image(sqlite3 *db,
                    const std::map<std::string, std::string> &params) {
  std::stringstream response;
  response << "HTTP/1.1 303 See Other\r\n";
  response << "Location: /\r\n\r\n";

  if (params.find("id") == params.end()) {
    return response.str();
  }

  int id = std::stoi(params.at("id"));

  // Get image info
  sqlite3_stmt *stmt;
  const char *sql = "SELECT original_url FROM images WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return response.str();
  }

  sqlite3_bind_int(stmt, 1, id);

  std::string original_url;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *url = (const char *)sqlite3_column_text(stmt, 0);
    original_url = url ? url : "";
  }

  sqlite3_finalize(stmt);

  // Delete from GCS if it's a GCS path
  if (original_url.find("gs://") == 0) {
    std::string cmd = "sudo gsutil rm " + original_url;
    exec_command(cmd);
  }

  // Delete record
  sql = "DELETE FROM images WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return response.str();
  }

  sqlite3_bind_int(stmt, 1, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return response.str();
}

// Handle delete video request
std::string
handle_delete_video(sqlite3 *db,
                    const std::map<std::string, std::string> &params) {
  std::stringstream response;
  response << "HTTP/1.1 303 See Other\r\n";
  response << "Location: /\r\n\r\n";

  if (params.find("id") == params.end()) {
    return response.str();
  }

  int id = std::stoi(params.at("id"));

  // Get video info
  sqlite3_stmt *stmt;
  const char *sql = "SELECT gcs_path FROM videos WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return response.str();
  }

  sqlite3_bind_int(stmt, 1, id);

  std::string gcs_path;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *path = (const char *)sqlite3_column_text(stmt, 0);
    gcs_path = path ? path : "";
  }

  sqlite3_finalize(stmt);

  // Delete from GCS
  if (!gcs_path.empty()) {
    std::string cmd = "sudo gsutil rm " + gcs_path;
    exec_command(cmd);
  }

  // Delete record
  sql = "DELETE FROM videos WHERE id = ?";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    return response.str();
  }

  sqlite3_bind_int(stmt, 1, id);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  return response.str();
}

// Parse URL parameters
std::map<std::string, std::string> parse_url_params(const std::string &url) {
  std::map<std::string, std::string> params;
  size_t pos = url.find('?');

  if (pos != std::string::npos) {
    std::string query = url.substr(pos + 1);
    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
      size_t eq_pos = pair.find('=');
      if (eq_pos != std::string::npos) {
        std::string key = pair.substr(0, eq_pos);
        std::string value = pair.substr(eq_pos + 1);
        params[key] = value;
      }
    }
  }

  return params;
}

// Extract content type and boundary from Content-Type header
bool parse_content_type(const std::string &header, std::string &content_type,
                        std::string &boundary) {
  size_t pos = header.find("Content-Type: ");
  if (pos == std::string::npos) {
    return false;
  }

  size_t start = pos + 14;
  size_t end = header.find("\r\n", start);
  std::string content_type_line = header.substr(start, end - start);

  pos = content_type_line.find(';');
  if (pos == std::string::npos) {
    content_type = content_type_line;
    return false;
  }

  content_type = content_type_line.substr(0, pos);

  pos = content_type_line.find("boundary=");
  if (pos == std::string::npos) {
    return false;
  }

  boundary = content_type_line.substr(pos + 9);

  // Remove quotes if present
  if (boundary.front() == '"' && boundary.back() == '"') {
    boundary = boundary.substr(1, boundary.length() - 2);
  }

  return true;
}

// Main request handler
void handle_request(int client_socket, const char *request) {
  // Open database connection
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

  // Parse request
  std::string req(request);
  std::string method, path;

  // Extract method and path
  size_t pos = req.find(' ');
  if (pos != std::string::npos) {
    method = req.substr(0, pos);
    size_t path_end = req.find(' ', pos + 1);
    if (path_end != std::string::npos) {
      path = req.substr(pos + 1, path_end - (pos + 1));
    }
  }

  // Parse base path (without query parameters)
  std::string base_path = path;
  size_t query_pos = path.find('?');
  if (query_pos != std::string::npos) {
    base_path = path.substr(0, query_pos);
  }

  // Parse query parameters
  std::map<std::string, std::string> params = parse_url_params(path);

  // Find request body (after the headers)
  size_t body_pos = req.find("\r\n\r\n");
  std::string body;
  if (body_pos != std::string::npos) {
    body = req.substr(body_pos + 4);
  }

  // Parse headers
  std::string content_type, boundary;
  parse_content_type(req, content_type, boundary);

  // Handle different paths
  std::string response;

  if (method == "GET") {
    if (base_path == "/" || base_path == "/index") {
      response = "HTTP/1.1 200 OK\r\n";
      response += "Content-Type: text/html\r\n\r\n";
      response += generate_main_page(db);
    } else if (base_path == "/delete-image") {
      response = handle_delete_image(db, params);
    } else if (base_path == "/delete-video") {
      response = handle_delete_video(db, params);
    } else {
      response = "HTTP/1.1 404 Not Found\r\n";
      response += "Content-Type: text/plain\r\n\r\n";
      response += "404 - Page not found";
    }
  } else if (method == "POST") {
    if (base_path == "/upload-image" && content_type == "multipart/form-data" &&
        !boundary.empty()) {
      // Parse form data and handle image upload
      std::map<std::string, std::vector<char>> files;
      std::map<std::string, std::string> form_data =
          parse_multipart_form_data(body, boundary, files);
      response = handle_image_upload(db, form_data, files);
    } else if (base_path == "/upload-video" &&
               content_type == "multipart/form-data" && !boundary.empty()) {
      // Parse form data and handle video upload
      std::map<std::string, std::vector<char>> files;
      std::map<std::string, std::string> form_data =
          parse_multipart_form_data(body, boundary, files);
      response = handle_video_upload(db, form_data, files);
    } else {
      response = "HTTP/1.1 400 Bad Request\r\n";
      response += "Content-Type: text/plain\r\n\r\n";
      response += "400 - Bad Request";
    }
  } else {
    response = "HTTP/1.1 405 Method Not Allowed\r\n";
    response += "Content-Type: text/plain\r\n\r\n";
    response += "405 - Method Not Allowed";
  }

  // Close database connection and send response
  sqlite3_close(db);
  send(client_socket, response.c_str(), response.length(), 0);
}

std::string read_full_http_request(int client_socket) {
  std::string request_data;
  char buffer[BUFFER_SIZE];
  int bytes_read;

  log_to_file("Started reading HTTP request");

  // Read headers first
  while ((bytes_read = read(client_socket, buffer, sizeof(buffer))) > 0) {
    request_data.append(buffer, bytes_read);

    // Check if we have received complete headers
    size_t header_end = request_data.find("\r\n\r\n");
    if (header_end != std::string::npos) {
      break;
    }
  }

  if (bytes_read <= 0) {
    log_to_file("Error reading request headers");
    return request_data; // Error or connection closed
  }

  // Parse Content-Length
  int content_length = 0;
  size_t pos = request_data.find("Content-Length:");
  if (pos != std::string::npos) {
    std::string cl_str = request_data.substr(pos + 15);
    pos = cl_str.find_first_of("\r\n");
    if (pos != std::string::npos) {
      cl_str = cl_str.substr(0, pos);
    }
    // Trim leading whitespace
    cl_str.erase(0, cl_str.find_first_not_of(" \t"));
    content_length = std::stoi(cl_str);
    log_to_file("Content-Length detected: " + std::to_string(content_length) +
                " bytes");
  }

  // Calculate how much body data we've already read
  size_t header_end = request_data.find("\r\n\r\n");
  size_t body_received = request_data.size() - (header_end + 4);
  log_to_file("Already received " + std::to_string(body_received) +
              " bytes of body data");

  // Read the rest of the body
  while (body_received < (size_t)content_length) {
    bytes_read = read(client_socket, buffer, sizeof(buffer));
    if (bytes_read > 0) {
      request_data.append(buffer, bytes_read);
      body_received += bytes_read;
      log_to_file("Read " + std::to_string(bytes_read) +
                  " more bytes, total body: " + std::to_string(body_received) +
                  "/" + std::to_string(content_length));
    } else if (bytes_read == 0) {
      log_to_file("Connection closed before receiving complete body");
      break; // Connection closed
    } else {
      log_to_file("Error reading request body: " + std::to_string(errno));
      break; // Error
    }
  }

  log_to_file("Finished reading HTTP request, total size: " +
              std::to_string(request_data.size()) + " bytes");
  return request_data;
}

// Add these functions for better debugging

int main() {
  // Create directory for temporary uploads
  system(("mkdir -p " + std::string(TEMP_UPLOAD_DIR)).c_str());

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
  address.sin_port = htons(MEDIA_PORT);

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

  printf("Media Manager Server started on localhost:%d\n", MEDIA_PORT);

  while (1) {
    if ((new_socket =
             accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }

    memset(buffer, 0, BUFFER_SIZE);
    std::string request_data;
    std::string full_request = read_full_http_request(new_socket);
    handle_request(new_socket, full_request.c_str());

    close(new_socket);
  }

  close(server_fd);
  return 0;
}
