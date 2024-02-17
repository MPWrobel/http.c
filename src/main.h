typedef enum {
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	CONNECT,
	OPTIONS,
	TRACE,
	PATCH
} RequestMethod;

typedef struct {
	char *name;
	char *value;
} RequestHeader;

typedef struct {
	RequestMethod  method;
	char          *url;
	char          *version;
	RequestHeader *headers;
	char          *body;
} Request;

typedef struct {
	char *key;
	char *value;
} FileType;

RequestMethod ParseRequestMethod(char *);
RequestHeader ParseRequestHeader(char *);
