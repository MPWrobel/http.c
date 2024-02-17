#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define STB_DS_IMPLEMENTATION
#include "main.h"
#include "stb_ds.h"

// clang-format off
FileType *file_types;
static FileType _file_types[] = {
 /* application */
	{"7z", "application/x-7z-compressed"},
	{"abw", "application/x-abiword"},
	{"arc", "application/x-freearc"},
	{"azw", "application/vnd.amazon.ebook"},
	{"bin", "application/octet-stream"},
	{"bz", "application/x-bzip"},
	{"bz2", "application/x-bzip2"},
	{"cda", "application/x-cdf"},
	{"csh", "application/x-csh"},
	{"doc", "application/msword"},
	{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
	{"eot", "application/vnd.ms-fontobject"},
	{"epub", "application/epub+zip"},
	{"gz", "application/gzip"},
	{"jar", "application/java-archive"},
	{"json", "application/json"},
	{"jsonld", "application/ld+json"},
	{"mpkg", "application/vnd.apple.installer+xml"},
	{"odp", "application/vnd.oasis.opendocument.presentation"},
	{"ods", "application/vnd.oasis.opendocument.spreadsheet"},
	{"odt", "application/vnd.oasis.opendocument.text"},
	{"ogx", "application/ogg"},
	{"pdf", "application/pdf"},
	{"php", "application/x-httpd-php"},
	{"ppt", "application/vnd.ms-powerpoint"},
	{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
	{"rar", "application/vnd.rar"},
	{"rtf", "application/rtf"},
	{"sh", "application/x-sh"},
	{"tar", "application/x-tar"},
	{"vsd", "application/vnd.visio"},
	{"xhtml", "application/xhtml+xml"},
	{"xls", "application/vnd.ms-excel"},
	{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
	{"xml", "application/xml"},
	{"xul", "application/vnd.mozilla.xul+xml"},
	{"zip", "application/zip"},
 /* text */
	{"c", "text/plain"},
	{"h", "text/plain"},
	{"css", "text/css"},
	{"csv", "text/csv"},
	{"htm", "text/html"},
	{"html", "text/html"},
	{"ics", "text/calendar"},
	{"js", "text/javascript"},
	{"mjs", "text/javascript"},
	{"txt", "text/plain"},
 /* audio */
	{"aac", "audio/aac"},
	{"mid", "audio/midi"},
	{"midi", "audio/midi"},
	{"mp3", "audio/mpeg"},
	{"oga", "audio/ogg"},
	{"opus", "audio/opus"},
	{"wav", "audio/wav"},
	{"weba", "audio/webm"},
 /* font */
	{"otf", "font/otf"},
	{"ttf", "font/ttf"},
	{"woff", "font/woff"},
	{"woff2", "font/woff2"},
 /* image */
	{"apng", "image/apng"},
	{"avif", "image/avif"},
	{"bmp", "image/bmp"},
	{"gif", "image/gif"},
	{"ico", "image/vnd.microsoft.icon"},
	{"jpg", "image/jpeg"},
	{"jpeg", "image/jpeg"},
	{"png", "image/png"},
	{"svg", "image/svg+xml"},
	{"tif", "image/tiff"},
	{"tiff", "image/tiff"},
	{"webp", "image/webp"},
 /* video */
	{"3g2", "video/3gpp2"},
	{"3gp", "video/3gpp"},
	{"avi", "video/x-msvideo"},
	{"mp4", "video/mp4"},
	{"mpeg", "video/mpeg"},
	{"ogv", "video/ogg"},
	{"ts", "video/mp2t"},
	{"webm", "video/webm"},
};
// clang-format on

char *dir_listing_begin = "<!DOCTYPE HTML>\n"
						  "<html lang=\"en\">\n"
						  "<head>\n"
						  "<meta charset=\"utf-8\">\n"
						  "<title>Directory listing for /</title>\n"
						  "</head>\n"
						  "<body>\n"
						  "<h1>Directory listing for /</h1>\n"
						  "<hr>\n"
						  "<ul>\n";
char *dir_listing_end   = "</ul>\n"
						  "<hr>\n"
						  "</body>\n"
						  "</html>";
char *not_found         = "<!DOCTYPE HTML>\n"
						  "<html lang=\"en\">\n"
						  "<head>\n"
						  "<meta charset=\"utf-8\">\n"
						  "<title>404 not found</title>\n"
						  "</head>\n"
						  "<body>\n"
						  "<h1>404 not found</h1>\n"
						  "</body>\n"
						  "</html>";

RequestMethod
ParseRequestMethod(char *string)
{
	if (strcmp(string, "CONNECT") == 0) return CONNECT;
	if (strcmp(string, "DELETE") == 0) return DELETE;
	if (strcmp(string, "GET") == 0) return GET;
	if (strcmp(string, "HEAD") == 0) return HEAD;
	if (strcmp(string, "OPTIONS") == 0) return OPTIONS;
	if (strcmp(string, "PATCH") == 0) return PATCH;
	if (strcmp(string, "POST") == 0) return POST;
	if (strcmp(string, "PUT") == 0) return PUT;
	if (strcmp(string, "TRACE") == 0) return TRACE;

	fprintf(stderr, "error: unknown method %s\n", string);
	exit(2);
}

RequestHeader
ParseRequestHeader(char *line)
{
	char *name = strsep(&line, ":");
	while (*line == ' ') line++;
	return (RequestHeader){.name = name, .value = line};
}

Request
ParseRequest(char *data)
{
	Request request = {0};

	char *next = data;
	char *line = strsep(&next, "\n");
	line       = strsep(&line, "\r");

	request.method  = ParseRequestMethod(strsep(&line, " "));
	request.url     = strsep(&line, " ");
	request.version = strsep(&line, " ");

	while ((line = strsep(&next, "\n")) && strlen(line)) {
		line = strsep(&line, "\r");
		if (*line == '\0') break;
		arrpush(request.headers, ParseRequestHeader(line));
	}

	request.body = next;

	return request;
}

char *
RecieveData(int sock)
{
	char *data = NULL;
	int   len  = 0;
	char  buf[512];
	for (int bytes_read = read(sock, buf, sizeof(buf)); bytes_read > 0;
	     bytes_read     = recv(sock, buf, sizeof(buf), MSG_DONTWAIT)) {
		data = realloc(data, len + bytes_read + 1);
		memcpy(data + len, buf, bytes_read);
		data[len + bytes_read] = '\0';
		len += bytes_read;
	}

	return data;
}

void
SendFile(int sock, char *path)
{
	int file = open(path, O_RDONLY);

	if (file != -1) {
		// TODO: treat the last dot-separated substring as the extension
		while (*path == '.') path++;
		char *filename  = strsep(&path, ".");
		char *extension = strsep(&path, ".");

		dprintf(sock, "HTTP/1.1 200\r\n");
		if (extension) {
			dprintf(sock, "Content-Type: %s\r\n", shget(file_types, extension));
		}
		dprintf(sock, "\r\n");

		int size = lseek(file, 0, SEEK_END);
		lseek(file, 0, SEEK_SET);

		char *buf = malloc(size);
		read(file, buf, size);
		send(sock, buf, size, 0);

		close(file);
	} else {
		dprintf(sock, "HTTP/1.1 404\r\n");
		dprintf(sock, "Content-Type: %s\r\n", shget(file_types, "html"));
		dprintf(sock, "\r\n");
		dprintf(sock, "%s", not_found);
	}
}

void
SendDirectory(int sock, char *path)
{
	if (access("index.html", R_OK) == 0) {
		char  *index_name     = "index.html";
		size_t index_path_len = strlen(path) + strlen(index_name) + 2;
		char  *index_path     = malloc(index_path_len);
		snprintf(index_path, index_path_len, "%s/%s", path, index_name);
		SendFile(sock, index_path);
		free(index_path);
		return;
	}

	DIR *dir = opendir(path);

	if (dir != NULL) {
		dprintf(sock, "HTTP/1.1 200\r\n");
		dprintf(sock, "Content-Type: %s\r\n", shget(file_types, "html"));
		dprintf(sock, "\r\n");

		dprintf(sock, "%s", dir_listing_begin);

		struct dirent *entry;
		while ((entry = readdir(dir))) {
			bool slash = path[strlen(path) - 1] == '/';
			dprintf(sock, "<li><a href=\"%s%s%s\">%s</a></li>\n",
			        slash ? "" : path, slash ? "" : "/", entry->d_name,
			        entry->d_name);
		}

		dprintf(sock, "%s", dir_listing_end);
		closedir(dir);
	} else {
		dprintf(sock, "HTTP/1.1 404\r\n");
		dprintf(sock, "Content-Type: %s\r\n", shget(file_types, "html"));
		dprintf(sock, "\r\n");
		dprintf(sock, "%s", not_found);
	}
}

void
PrintRequest(Request *request)
{
	printf("METHOD: [%d]\n", request->method);
	printf("URL: [%s]\n", request->url);
	printf("VERSION: [%s]\n", request->version);
	for (int i = 0; i < arrlen(request->headers); i++) {
		RequestHeader header = request->headers[i];
		printf("HEADER: [%s]: [%s]\n", header.name, header.value);
	}
}

int
BindPort(int port)
{
	int sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

	struct sockaddr_in addr = {.sin_family      = PF_INET,
	                           .sin_port        = htons(port),
	                           .sin_addr.s_addr = INADDR_ANY};

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		fprintf(stderr, "error: failed to bind port %d\n", port);
		exit(errno);
	}

	return sock;
}

int
main(int argc, char **argv)
{
	int port;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	if ((port = atoi(argv[1])) == 0) {
		fprintf(stderr, "error: invalid port %s \n", argv[1]);
		exit(2);
	}

	shdefault(file_types, "application/octet-stream");
	for (int i = 0; i < sizeof(_file_types) / sizeof(*_file_types); i++) {
		shputs(file_types, _file_types[i]);
	}

	int host = BindPort(port);
	listen(host, 0);

	printf("Listening on port %d\n", port);
	fflush(stdout);

	for (;;) {
		int   client = accept(host, NULL, NULL);
		char *data   = RecieveData(client);
		puts(data);

		Request request = ParseRequest(data);
		PrintRequest(&request);

		struct stat info;
		char       *path = request.url;
		while (*path == '/') path++;
		if (*path == '\0') path = ".";
		stat(path, &info);
		if (S_ISDIR(info.st_mode)) SendDirectory(client, path);
		else SendFile(client, path);

		close(client);
		free(data);
	}

	close(host);
}
