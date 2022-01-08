#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <archive.h>
#include <archive_entry.h>

struct multi_userdata {
	char *data;
	size_t data_len;

	CURLM* mhnd;
	size_t statistics_max_buflen;
};

size_t write_callback(char *ptr, size_t size, size_t nmemb, struct multi_userdata *mu)
{
	size_t oldlength = mu->data_len;
	size_t newlength = oldlength + (size * nmemb);

	mu->data_len = newlength;
	if ((size * nmemb) == 0) {
		return 0;
	}

	mu->data = realloc(mu->data, mu->data_len);

	memcpy(mu->data + oldlength, ptr, size * nmemb);

	if (newlength > mu->statistics_max_buflen) {
		mu->statistics_max_buflen = newlength;
	}

	return size * nmemb;
}

int fetch(struct multi_userdata *mu)
{
	int count_running = -1;

	do {
		CURLMcode mc = curl_multi_perform(mu->mhnd, &count_running);

		if (!mc && count_running > 0) {
			mc = curl_multi_poll(mu->mhnd, NULL, 0, 1000, NULL);

			if (mc) {
				fprintf(stderr, "curl_multi_poll failed: %d\n", (int)mc);
				return -1;
			}
		}
	} while(count_running > 0 && mu->data_len == 0);

	return count_running;
}

ssize_t curl_read(struct archive *a, void *userdata, const void **buff)
{
	struct multi_userdata *mu = (struct multi_userdata*) userdata;

	mu->data_len = 0;

	fetch(mu);

	*buff = mu->data;

	return mu->data_len;
}

int main(int argc, char **argv)
{
	struct multi_userdata mu;

	mu.data = NULL;
	mu.data_len = 0;
	mu.statistics_max_buflen = 0;

	CURL *hnd;

	hnd = curl_easy_init();
	//curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 102400L);
	curl_easy_setopt(hnd, CURLOPT_BUFFERSIZE, 128L);
	curl_easy_setopt(hnd, CURLOPT_URL, "https://curl.se/download/curl-7.81.0.tar.gz");
	//curl_easy_setopt(hnd, CURLOPT_URL, "https://curl.se/download/curl-7.81.0.zip");
	curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl+libarchive");
	curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
	curl_easy_setopt(hnd, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
	curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);

	curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &mu);

	mu.mhnd = curl_multi_init();
	curl_multi_add_handle(mu.mhnd, hnd);

	struct archive *a;
	struct archive_entry *entry;

	a = archive_read_new();
	if (archive_read_support_format_all(a) != 0) {
		fprintf(stderr, "archive_read_support_format_all failed\n");
	}
	if (archive_read_support_filter_all(a) != 0) {
		fprintf(stderr, "archive_read_support_filter_all failed\n");
	}
	if (archive_read_open(a, &mu, NULL, curl_read, NULL) != 0) {
		fprintf(stderr, "archive_read_open failed\n");
		fprintf(stderr, "archive_ret: %s\n", archive_error_string(a));
	}

	int archive_ret = -1;

	do {
		archive_ret = archive_read_next_header(a, &entry);
		if (archive_ret == ARCHIVE_OK)
			printf("%s\n",archive_entry_pathname(entry));
	} while(archive_ret == ARCHIVE_OK || archive_ret == ARCHIVE_RETRY);
	
	printf("archive_ret: %d\n", archive_ret);
	printf("archive_ret: %s\n", archive_error_string(a));
	printf("maximum buflen was: %ld\n", mu.statistics_max_buflen);

	free(mu.data);
	curl_easy_cleanup(hnd);
	curl_multi_cleanup(mu.mhnd);

	archive_read_free(a);

	return 0;
}
