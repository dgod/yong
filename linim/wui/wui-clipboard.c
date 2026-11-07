// Wayland clipboard implementation.
//
// The text of another client's selection is fetched asynchronously right after
// the compositor notifies us about a new selection and is then cached in a
// static buffer of 512 bytes (the same model as the GTK/X11 backend).
// w_clipboard_get_text() only reads that cache, so it never has to run a
// nested display roundtrip from inside the main loop - such a roundtrip may
// deadlock when the call happens while compositor events are being dispatched
// (e.g. from a key event handler).

#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include <glib-unix.h>

// Cached selection text, 512 bytes including the terminating NUL
#define W_CLIPBOARD_MAX_SIZE	512

static char wl_clipboard_text[W_CLIPBOARD_MAX_SIZE];

// An in-flight receive.  The fd is non-blocking and drained by the main loop,
// never by w_clipboard_get_text().  A transfer can outlive its usefulness:
// when a new selection arrives while the previous one is still being read we
// must not close the old read end, because its source is still writing and
// would fail with EPIPE (the protocol tells the receiver to read until EOF).
// Such transfers keep running as drain-only until they finish on their own.
typedef struct {
	int fd;
	guint source_id;
	bool used;
	bool cache;			// keep the text when the transfer completes
	char buf[W_CLIPBOARD_MAX_SIZE];
	int size;
	bool too_large;
} clipboard_transfer_t;

#define CLIPBOARD_TRANSFER_MAX	4

static clipboard_transfer_t wl_clipboard_transfers[CLIPBOARD_TRANSFER_MAX] = {
	{-1}, {-1}, {-1}, {-1},
};

// The transfer the cached text comes from (if any)
static clipboard_transfer_t *wl_clipboard_current = NULL;

// Replace the cached selection text (truncated to the buffer size)
static void clipboard_cache_set(const void *data,int size)
{
	int len=size<W_CLIPBOARD_MAX_SIZE-1?size:W_CLIPBOARD_MAX_SIZE-1;
	if(len<0)
		len=0;
	memcpy(wl_clipboard_text,data,len);
	wl_clipboard_text[len]=0;
}

static void clipboard_cache_clear(void)
{
	wl_clipboard_text[0]=0;
}

static void clipboard_transfer_reset(clipboard_transfer_t *t)
{
	t->fd = -1;
	t->source_id = 0;
	t->used = false;
	t->cache = false;
	t->size = 0;
	t->too_large = false;
}

// Stop a transfer and give up its file descriptor
static void clipboard_transfer_free(clipboard_transfer_t *t)
{
	if(t->source_id)
		g_source_remove(t->source_id);
	if(t->fd>=0)
		close(t->fd);
	if(t==wl_clipboard_current)
		wl_clipboard_current = NULL;
	clipboard_transfer_reset(t);
}

// Stop caching the text of the running transfer, but let it drain to EOF
static void clipboard_transfer_detach(void)
{
	if(!wl_clipboard_current)
		return;

	wl_clipboard_current->cache = false;
	wl_clipboard_current->size = 0;
	wl_clipboard_current->too_large = false;
	wl_clipboard_current = NULL;
}

// Pick a free slot; if all are busy (a source that never closes its end, or a
// storm of selections) drop a drain-only transfer, never the current one
static clipboard_transfer_t *clipboard_transfer_alloc(void)
{
	for(int i=0;i<CLIPBOARD_TRANSFER_MAX;i++)
	{
		if(!wl_clipboard_transfers[i].used)
			return &wl_clipboard_transfers[i];
	}
	for(int i=0;i<CLIPBOARD_TRANSFER_MAX;i++)
	{
		if(&wl_clipboard_transfers[i]==wl_clipboard_current)
			continue;
		clipboard_transfer_free(&wl_clipboard_transfers[i]);
		return &wl_clipboard_transfers[i];
	}
	return NULL;
}

// Main loop callback: drain the pipe the compositor writes the text into
static gboolean clipboard_transfer_read(gint fd,GIOCondition condition,void *user_data)
{
	clipboard_transfer_t *t = user_data;
	char buf[W_CLIPBOARD_MAX_SIZE];

	for(;;)
	{
		ssize_t n=read(fd,buf,sizeof(buf));
		if(n>0)
		{
			if(!t->cache)
			{
				// drain-only transfer, throw the data away
			}
			else if(t->size+n>W_CLIPBOARD_MAX_SIZE-1)
			{
				// Way too much data for a selection text, drop it
				t->too_large = true;
			}
			else
			{
				memcpy(t->buf+t->size,buf,n);
				t->size += n;
			}
			continue;
		}
		if(n==0)
			break;				// EOF: the sender closed its end
		if(errno==EINTR)
			continue;
		if(errno==EAGAIN || errno==EWOULDBLOCK)
			return G_SOURCE_CONTINUE;	// more data will follow
		break;					// read error
	}

	// Transfer done: cache the text (the source removes itself below).
	// Oversized or empty data clears the cache, like the X11 backend does.
	if(t->cache)
	{
		if(t->too_large)
		{
			clipboard_cache_clear();
			if(wl_debug)
				fprintf(stderr, "[wui-wayland] clipboard text exceeds %d bytes, ignored\n",
					W_CLIPBOARD_MAX_SIZE-1);
		}
		else
		{
			t->buf[t->size] = 0;
			strcpy(wl_clipboard_text,t->buf);
		}
	}

	if(t==wl_clipboard_current)
		wl_clipboard_current = NULL;

	close(fd);
	clipboard_transfer_reset(t);

	return G_SOURCE_REMOVE;
}

// Start fetching the text of the offer that just became the selection.
// `ext` tells which clipboard protocol the offer belongs to, `mime` is the
// text mime type to ask for.
static void clipboard_read_start(void *offer,bool ext,const char *mime)
{
	// The text is about to change: keep whatever was in flight draining, but
	// forget what it cached
	clipboard_transfer_detach();
	clipboard_cache_clear();

	if(!offer)
		return;

	clipboard_transfer_t *t=clipboard_transfer_alloc();
	if(!t)
		return;

	int fd[2];
	if(pipe(fd)==-1)
		return;

	// The read end is drained from the main loop, so it must not block
	int flags=fcntl(fd[0],F_GETFL,0);
	if(flags!=-1)
		fcntl(fd[0],F_SETFL,flags|O_NONBLOCK);

	if(ext)
		ext_data_control_offer_v1_receive(offer,mime,fd[1]);
	else
		wl_data_offer_receive(offer,mime,fd[1]);
	close(fd[1]);

	// No flush/roundtrip here: the receive request is sent out by the wayland
	// GSource when the main loop runs again, and the compositor then writes
	// the text into the pipe, which wakes up clipboard_transfer_read().

	clipboard_transfer_reset(t);
	t->fd = fd[0];
	t->cache = true;
	t->used = true;
	t->source_id = g_unix_fd_add(fd[0],G_IO_IN|G_IO_HUP|G_IO_ERR,
		clipboard_transfer_read,t);
	if(!t->source_id)
	{
		close(fd[0]);
		clipboard_transfer_reset(t);
		return;
	}
	wl_clipboard_current = t;

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] fetching clipboard text asynchronously ('%s')\n", mime);
}

// ---- offered text mime types ----
// The text has to be requested as explicitly UTF-8: several toolkits (GDK in
// particular) serve a charset-less "text/plain" through an ASCII charset
// converter whose fallback replaces non-ASCII bytes by escapes, so a Chinese
// character arrives as "\u847a" instead of its UTF-8 encoding, while
// "text/plain;charset=utf-8"/"UTF8_STRING" hand out the raw UTF-8 bytes.
static const char *clipboard_text_mimes[] = {
	"text/plain;charset=utf-8",
	"UTF8_STRING",
	"text/plain",
};
#define CLIPBOARD_TEXT_MIME_COUNT	(int)(sizeof(clipboard_text_mimes)/sizeof(clipboard_text_mimes[0]))
#define CLIPBOARD_TEXT_MIME_DEFAULT	clipboard_text_mimes[CLIPBOARD_TEXT_MIME_COUNT-1]

// Offer the next selection event will refer to, and the best text mime type
// seen for it (mime_type events arrive before the selection event)
static void *wl_offer_candidate = NULL;
static int wl_offer_mime_index = -1;

static void clipboard_offer_reset(void *offer)
{
	wl_offer_candidate = offer;
	wl_offer_mime_index = -1;
}

static void clipboard_offer_add_mime(void *offer,const char *mime)
{
	if(offer != wl_offer_candidate || !mime)
		return;

	for(int i=0;i<CLIPBOARD_TEXT_MIME_COUNT;i++)
	{
		if(strcasecmp(mime,clipboard_text_mimes[i])!=0)
			continue;
		if(wl_offer_mime_index<0 || i<wl_offer_mime_index)
			wl_offer_mime_index = i;
		return;
	}
}

// Best text mime type to request from the given offer
static const char *clipboard_offer_text_mime(void *offer)
{
	if(offer && offer==wl_offer_candidate && wl_offer_mime_index>=0)
		return clipboard_text_mimes[wl_offer_mime_index];
	return CLIPBOARD_TEXT_MIME_DEFAULT;
}

static void clipboard_offer_mime_ext(void *data,
	struct ext_data_control_offer_v1 *offer,const char *mime)
{
	clipboard_offer_add_mime(offer,mime);
}

static const struct ext_data_control_offer_v1_listener clipboard_ext_offer_listener = {
	.offer = clipboard_offer_mime_ext,
};

static void clipboard_offer_mime_std(void *data,
	struct wl_data_offer *offer,const char *mime)
{
	clipboard_offer_add_mime(offer,mime);
}

// Only sent for drag-and-drop offers, which we do not use
static void clipboard_offer_source_actions(void *data,
	struct wl_data_offer *offer,uint32_t source_actions)
{
}

static void clipboard_offer_action(void *data,
	struct wl_data_offer *offer,uint32_t dnd_action)
{
}

static const struct wl_data_offer_listener clipboard_std_offer_listener = {
	.offer = clipboard_offer_mime_std,
	.source_actions = clipboard_offer_source_actions,
	.action = clipboard_offer_action,
};

// ---- MIME type matching helper ----
// Returns true if the requested mime matches the stored mime.  Our text data
// is always UTF-8, so the usual text spellings are interchangeable.
static bool is_text_mime(const char *mime)
{
	if(!mime)
		return false;
	for(int i=0;i<CLIPBOARD_TEXT_MIME_COUNT;i++)
	{
		if(strcasecmp(mime,clipboard_text_mimes[i])==0)
			return true;
	}
	return false;
}

static bool mime_text_match(const char *requested, const char *stored)
{
	if(!requested || !stored)
		return false;
	if(strcmp(requested, stored) == 0)
		return true;
	// text/plain, text/plain;charset=utf-8 and UTF8_STRING carry the same bytes
	return is_text_mime(requested) && is_text_mime(stored);
}

static void data_control_source_send(void *data,
	struct ext_data_control_source_v1 *ext_data_control_source_v1,
	const char *mime_type,
	int32_t fd)
{
	// Check if requested MIME type matches what we have
	if(!wl_data_control_source_data.mime || !wl_data_control_source_data.data ||
	   wl_data_control_source_data.size <= 0 ||
	   !mime_text_match(mime_type, wl_data_control_source_data.mime))
	{
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] data control source send: unsupported mime '%s'\n", mime_type);
		close(fd);
		return;
	}

	// Write data to compositor's fd
	ssize_t written = 0;
	ssize_t total = 0;
	while(total < wl_data_control_source_data.size)
	{
		written = write(fd, wl_data_control_source_data.data + total,
					wl_data_control_source_data.size - total);
		if(written <= 0)
			break;
		total += written;
	}
	close(fd);

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] data control source send: %s, wrote %zd bytes\n",
			mime_type, total);
}

static void data_control_source_cancelled(void *data,
	struct ext_data_control_source_v1 *ext_data_control_source_v1)
{
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] data control source cancelled\n");

	// Clean up the source — compositor no longer holds it
	if(ext_data_control_source_v1 == wl_active_source)
		wl_active_source = NULL;
	ext_data_control_source_v1_destroy(ext_data_control_source_v1);

	// No longer own the selection
	wl_owns_clipboard = false;
}

static const struct ext_data_control_source_v1_listener data_control_source_listener = {
	.send = data_control_source_send,
	.cancelled = data_control_source_cancelled,
};

// Registry listeners
static void data_control_device_selection(void *data,
	struct ext_data_control_device_v1 *ext_data_control_device_v1,
	struct ext_data_control_offer_v1 *id)
{
	// The previous selection is gone: let its transfer drain to EOF
	clipboard_transfer_detach();

	if(wl_data_control_offer)
		ext_data_control_offer_v1_destroy(wl_data_control_offer);
	wl_data_control_offer = id;

	// Non-NULL offer means another client's data
	if(id != NULL)
		wl_owns_clipboard = false;

	// Fetch the text now and keep it cached for later w_clipboard_get_text()
	// calls (a NULL offer just clears the cache)
	clipboard_read_start(id,true,clipboard_offer_text_mime(id));

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] data control selection changed: %p\n", (void*)id);
}

static void data_control_device_finished(void *data,
	struct ext_data_control_device_v1 *ext_data_control_device_v1)
{
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] data control device finished\n");
}

static void data_control_device_data_offer(void *data,
			   struct ext_data_control_device_v1 *ext_data_control_device_v1,
			   struct ext_data_control_offer_v1 *id)
{
	if(wl_debug)
		fprintf(stderr,"[wui-wayland] data control device data offer\n");

	// Collect the offered mime types: the selection event that follows can
	// then ask for the best text one (see clipboard_offer_add_mime)
	clipboard_offer_reset(id);
	ext_data_control_offer_v1_add_listener(id,&clipboard_ext_offer_listener,NULL);
}

static void data_control_device_primary_selection(void *data,
				  struct ext_data_control_device_v1 *ext_data_control_device_v1,
				  struct ext_data_control_offer_v1 *id)
{
	if(wl_debug)
		fprintf(stderr,"[wui-wayland] data control device primary selection\n");
}

static const struct ext_data_control_device_v1_listener data_control_device_listener = {
	.data_offer = data_control_device_data_offer,
	.selection = data_control_device_selection,
	.finished = data_control_device_finished,
	.primary_selection = data_control_device_primary_selection,
};

// ---- Standard clipboard (wl_data_device) fallback ----

static void data_device_data_offer(void *data,
	struct wl_data_device *device, struct wl_data_offer *id)
{
	// Collect the offered mime types: the selection event that follows can
	// then ask for the best text one (see clipboard_offer_add_mime)
	clipboard_offer_reset(id);
	wl_data_offer_add_listener(id,&clipboard_std_offer_listener,NULL);
}

static void data_device_enter(void *data,
	struct wl_data_device *device, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
	struct wl_data_offer *id)
{
	// DND enter — not needed
	wl_data_offer_destroy(id);
}

static void data_device_leave(void *data,
	struct wl_data_device *device)
{
}

static void data_device_motion(void *data,
	struct wl_data_device *device, uint32_t time,
	wl_fixed_t x, wl_fixed_t y)
{
}

static void data_device_drop(void *data,
	struct wl_data_device *device)
{
}

static void data_device_selection_std(void *data,
	struct wl_data_device *device, struct wl_data_offer *id)
{
	// Track clipboard selection; the previous transfer drains to EOF
	clipboard_transfer_detach();

	if(wl_data_offer)
		wl_data_offer_destroy(wl_data_offer);
	wl_data_offer = id;

	// Fetch the text now and keep it cached for later w_clipboard_get_text()
	// calls (a NULL offer just clears the cache)
	clipboard_read_start(id,false,clipboard_offer_text_mime(id));

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] std clipboard selection changed: %p\n", (void*)id);
}

static const struct wl_data_device_listener data_device_listener = {
	.data_offer = data_device_data_offer,
	.enter = data_device_enter,
	.leave = data_device_leave,
	.motion = data_device_motion,
	.drop = data_device_drop,
	.selection = data_device_selection_std,
};

// wl_data_source listener for standard clipboard write
static void data_source_send_std(void *data,
	struct wl_data_source *source, const char *mime_type, int32_t fd)
{
	// Same data matching logic as ext-data-control version
	if(!wl_data_control_source_data.mime || !wl_data_control_source_data.data ||
	   wl_data_control_source_data.size <= 0 ||
	   !mime_text_match(mime_type, wl_data_control_source_data.mime))
	{
		if(wl_debug)
			fprintf(stderr, "[wui-wayland] std source send: unsupported mime '%s'\n", mime_type);
		close(fd);
		return;
	}

	ssize_t total = 0;
	while(total < wl_data_control_source_data.size)
	{
		ssize_t written = write(fd, wl_data_control_source_data.data + total,
				wl_data_control_source_data.size - total);
		if(written <= 0)
			break;
		total += written;
	}
	close(fd);

	if(wl_debug)
		fprintf(stderr, "[wui-wayland] std source send: %s, wrote %zd bytes\n", mime_type, total);
}

static void data_source_cancelled_std(void *data,
	struct wl_data_source *source)
{
	if(wl_debug)
		fprintf(stderr, "[wui-wayland] std data source cancelled\n");

	if(source == wl_data_source_active)
		wl_data_source_active = NULL;
	wl_data_source_destroy(source);

	// No longer own the selection
	wl_owns_clipboard = false;
}

static const struct wl_data_source_listener data_source_listener_std = {
	.send = data_source_send_std,
	.cancelled = data_source_cancelled_std,
};

// ---- w_clipboard_get_text ----
// Returns the cached selection text (caller must free), or NULL when there is
// no cached text.  The text is fetched asynchronously by the selection
// listeners above, so this function never talks to the compositor and never
// blocks the main loop.
char *w_clipboard_get_text(void)
{
	// Fast path: if we own the clipboard, return data from memory directly
	if(wl_owns_clipboard &&
	   wl_data_control_source_data.data &&
	   wl_data_control_source_data.size > 0 &&
	   wl_data_control_source_data.mime)
	{
		if(mime_text_match(wl_data_control_source_data.mime, "text/plain"))
			return l_memdup0(wl_data_control_source_data.data,wl_data_control_source_data.size);
		return NULL;
	}

	// Otherwise return what the selection listeners cached
	if(!wl_clipboard_text[0])
		return NULL;

	return l_strdup(wl_clipboard_text);
}

// ---- w_clipboard_set_mime ----
// Save data and mime for the send callback (shared by both clipboard paths).
int w_clipboard_set_mime(const char *mime, const void *data, int size)
{
	if(!mime || !data || size <= 0)
		return -1;

	// Path 1: ext-data-control (preferred — no serial/focus requirement)
	struct ext_data_control_manager_v1 *manager = w_wayland_get_interface("ext_data_control_manager_v1");
	if(manager && wl_data_control_device)
	{
		// Release previous source if still active
		if(wl_active_source)
		{
			ext_data_control_source_v1_destroy(wl_active_source);
			wl_active_source = NULL;
		}

		struct ext_data_control_source_v1 *source = ext_data_control_manager_v1_create_data_source(manager);
		if(source)
		{
			wl_active_source = source;

			// Set listener to handle send requests from compositor
			ext_data_control_source_v1_add_listener(source, &data_control_source_listener, NULL);

			// Add mime types
			ext_data_control_source_v1_offer(source, mime);
			if(strcmp(mime, "text/plain") == 0)
			{
				// Our text is UTF-8: offer it as such, plus the X11 style
				// name, so that clients don't fall back to a latin-1 view
				ext_data_control_source_v1_offer(source, "text/plain;charset=utf-8");
				ext_data_control_source_v1_offer(source, "UTF8_STRING");
			}

			// Store data for send callback
			if(wl_data_control_source_data.data)
				l_free(wl_data_control_source_data.data);
			wl_data_control_source_data.data = l_memdup(data, size);
			wl_data_control_source_data.size = size;

			// Save mime string for send callback matching
			if(wl_data_control_source_data.mime)
				l_free(wl_data_control_source_data.mime);
			wl_data_control_source_data.mime = l_strdup(mime);

			// Set as selection
			ext_data_control_device_v1_set_selection(wl_data_control_device, source);

			// Ensure compositor processes the new selection before returning
			wl_display_roundtrip(wl_display);

			wl_owns_clipboard = true;

			// Keep the cache in sync with what we just put on the clipboard
			// (the selection notification may not have arrived yet)
			clipboard_transfer_detach();
			clipboard_cache_set(data,size);

			if(wl_debug)
				fprintf(stderr, "[wui-wayland] set clipboard mime '%s' (%d bytes) via ext-data-control\n", mime, size);

			return 0;
		}
	}

	// Path 2: standard wl_data_device fallback (requires serial from user input)
	if(wl_data_device_manager && wl_data_device && wl_last_serial > 0)
	{
		// Release previous source if still active
		if(wl_data_source_active)
		{
			wl_data_source_destroy(wl_data_source_active);
			wl_data_source_active = NULL;
		}

		struct wl_data_source *source = wl_data_device_manager_create_data_source(wl_data_device_manager);
		if(source)
		{
			wl_data_source_active = source;

			// Set listener
			wl_data_source_add_listener(source, &data_source_listener_std, NULL);

			// Add mime types
			wl_data_source_offer(source, mime);
			// GNOME clipboard manager often expects UTF8_STRING alongside text/plain
			if(strcmp(mime, "text/plain") == 0)
			{
				// Our text is UTF-8: offer it as such, plus the X11 style
				// name, so that clients don't fall back to a latin-1 view
				wl_data_source_offer(source, "text/plain;charset=utf-8");
				wl_data_source_offer(source, "UTF8_STRING");
			}

			// Store data for send callback (reuses same storage as ext-data-control)
			if(wl_data_control_source_data.data)
				l_free(wl_data_control_source_data.data);
			wl_data_control_source_data.data = l_memdup(data, size);
			wl_data_control_source_data.size = size;

			if(wl_data_control_source_data.mime)
				l_free(wl_data_control_source_data.mime);
			wl_data_control_source_data.mime = l_strdup(mime);

			// Set as selection — requires a serial from a user input event
			wl_data_device_set_selection(wl_data_device, source, wl_last_serial);

			wl_display_roundtrip(wl_display);

			wl_owns_clipboard = true;

			// Keep the cache in sync with what we just put on the clipboard
			// (the selection notification may not have arrived yet)
			clipboard_transfer_detach();
			clipboard_cache_set(data,size);

			if(wl_debug)
				fprintf(stderr, "[wui-wayland] set clipboard mime '%s' (%d bytes) via std data device\n", mime, size);

			return 0;
		}
	}

	return -1;
}

int w_clipboard_set_text(const char *text)
{
	if(!text)
		return -1;

	// Reuse set_mime with text/plain
	return w_clipboard_set_mime("text/plain", text, strlen(text));
}
