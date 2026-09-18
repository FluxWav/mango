#include "mango/layout/overview.h"
#include "mango/common/server.h"
#include "mango/config/parse_config.h"
#include "mango/layout/arrange.h"
#include "mango/layout/horizontal.h"
#include "mango/layout/layout.h"
#include "mango/manage/client.h"
#include "mango/manage/monitor.h"
#include "mango/overview/overview.h"
#include <math.h>
#include <stdbool.h>

int compare_layout_items(const void *a, const void *b) {
	float area_a = ((const OvLayoutItem *)a)->area;
	float area_b = ((const OvLayoutItem *)b)->area;
	if (area_a < area_b)
		return 1;
	if (area_a > area_b)
		return -1;
	return 0;
}

void begin_jump_mode(Monitor *m) { m->is_jump_mode = 1; }

bool try_place(OvPlacedRect *placed, int placed_cnt, float w, float h,
			   float gap, float avail_w, float avail_h, OvPlacedRect *out,
			   OvPoint *cands, OvPoint *feas) {
	int cand_cnt = 0;
	cands[cand_cnt++] = (OvPoint){0.0f, 0.0f};

	for (int i = 0; i < placed_cnt; i++) {
		OvPlacedRect p = placed[i];
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y};
		cands[cand_cnt++] = (OvPoint){p.x, p.y + p.h + gap};
		cands[cand_cnt++] = (OvPoint){p.x + p.w + gap, p.y + p.h + gap};
	}

	int unique_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		bool dup = false;
		for (int j = 0; j < unique_cnt; j++) {
			if (fabs(cands[i].x - cands[j].x) < 0.5f &&
				fabs(cands[i].y - cands[j].y) < 0.5f) {
				dup = true;
				break;
			}
		}
		if (!dup)
			cands[unique_cnt++] = cands[i];
	}
	cand_cnt = unique_cnt;

	int feas_cnt = 0;
	for (int i = 0; i < cand_cnt; i++) {
		float cx = cands[i].x;
		float cy = cands[i].y;

		if (cx < 0 || cy < 0 || cx + w > avail_w || cy + h > avail_h)
			continue;

		bool overlap = false;
		for (int j = 0; j < placed_cnt; j++) {
			OvPlacedRect p = placed[j];
			if (!(cx + w + gap <= p.x || cx >= p.x + p.w + gap ||
				  cy + h + gap <= p.y || cy >= p.y + p.h + gap)) {
				overlap = true;
				break;
			}
		}
		if (!overlap) {
			feas[feas_cnt++] = (OvPoint){cx, cy};
		}
	}

	if (feas_cnt == 0)
		return false;

	int best = 0;
	for (int i = 1; i < feas_cnt; i++) {
		if (feas[i].y < feas[best].y ||
			(fabs(feas[i].y - feas[best].y) < 0.5f &&
			 feas[i].x < feas[best].x)) {
			best = i;
		}
	}

	out->x = feas[best].x;
	out->y = feas[best].y;
	out->w = w;
	out->h = h;
	return true;
}

// Centers each packed row on the widest row's axis without creating overlaps.
static void center_placed_rows(OvPlacedRect *placed, int n, float gap) {
	if (n <= 1)
		return;

	int *row_of = calloc(n, sizeof(int));
	float *row_y = calloc(n, sizeof(float));
	int *row_order = calloc(n, sizeof(int));
	if (!row_of || !row_y || !row_order) {
		free(row_of);
		free(row_y);
		free(row_order);
		return;
	}

	int row_cnt = 0;
	for (int i = 0; i < n; i++) {
		int r = -1;
		for (int j = 0; j < row_cnt; j++) {
			if (fabsf(placed[i].y - row_y[j]) < 0.5f) {
				r = j;
				break;
			}
		}
		if (r < 0) {
			r = row_cnt++;
			row_y[r] = placed[i].y;
		}
		row_of[i] = r;
	}

	for (int i = 0; i < row_cnt; i++)
		row_order[i] = i;
	for (int i = 1; i < row_cnt; i++) {
		int key = row_order[i];
		int j = i - 1;
		while (j >= 0 && row_y[row_order[j]] > row_y[key]) {
			row_order[j + 1] = row_order[j];
			j--;
		}
		row_order[j + 1] = key;
	}

	float grid_w = 0.0f;
	for (int r = 0; r < row_cnt; r++) {
		float left = 1e30f;
		float right = -1e30f;
		for (int i = 0; i < n; i++) {
			if (row_of[i] != r)
				continue;
			if (placed[i].x < left)
				left = placed[i].x;
			float e = placed[i].x + placed[i].w;
			if (e > right)
				right = e;
		}
		float span = right - left;
		if (span > grid_w)
			grid_w = span;
	}

	for (int oi = 0; oi < row_cnt; oi++) {
		int r = row_order[oi];
		float left = 1e30f;
		float right = -1e30f;
		for (int i = 0; i < n; i++) {
			if (row_of[i] != r)
				continue;
			if (placed[i].x < left)
				left = placed[i].x;
			float e = placed[i].x + placed[i].w;
			if (e > right)
				right = e;
		}
		float span = right - left;
		float delta = grid_w * 0.5f - (left + span * 0.5f);
		float lo = -1e30f;
		float hi = 1e30f;

		// Rows sharing a vertical band keep the gap and never swap sides.
		for (int i = 0; i < n; i++) {
			if (row_of[i] != r)
				continue;
			OvPlacedRect *a = &placed[i];
			for (int j = 0; j < n; j++) {
				OvPlacedRect *b = &placed[j];
				if (row_of[j] == r)
					continue;
				if (!(a->y + a->h + gap <= b->y || a->y >= b->y + b->h + gap)) {
					if (b->x < a->x) {
						float min_delta = b->x + b->w + gap - a->x;
						if (min_delta > lo)
							lo = min_delta;
					} else {
						float max_delta = b->x - gap - a->x - a->w;
						if (max_delta < hi)
							hi = max_delta;
					}
				}
			}
		}

		if (-left > lo)
			lo = -left;
		if (grid_w - right < hi)
			hi = grid_w - right;

		if (lo <= hi) {
			if (delta < lo)
				delta = lo;
			if (delta > hi)
				delta = hi;
			if (fabsf(delta) > 0.5f) {
				for (int i = 0; i < n; i++) {
					if (row_of[i] == r)
						placed[i].x += delta;
				}
			}
		}
	}

	free(row_of);
	free(row_y);
	free(row_order);
}

// Packs client_list (n items) into region using the same "largest-first,
// binary-search-on-scale" bin-packing as the original flat overview_scale,
// generalized to an arbitrary target box and client subset so it can be
// reused per-region by overview_scale_grouped. gap_inner is the spacing
// between individual windows inside region (overviewgappi in both callers).
void overview_pack_region(Client **client_list, int n, struct wlr_box region,
						  int32_t gap_inner) {
	if (n <= 0)
		return;

	OvLayoutItem *items = calloc(n, sizeof(OvLayoutItem));
	if (!items)
		return;

	for (int k = 0; k < n; k++) {
		Client *c = client_list[k];
		items[k].c = c;
		float w = c->overview_backup_geom.width;
		float h = c->overview_backup_geom.height;
		if (w <= 0 || h <= 0) {
			w = 100.0f;
			h = 100.0f;
		}
		items[k].orig_w = w;
		items[k].orig_h = h;
		items[k].area = w * h;
	}

	qsort(items, n, sizeof(OvLayoutItem), compare_layout_items);

	float max_avail_w = fmaxf(1.0f, (float)region.width);
	float max_avail_h = fmaxf(1.0f, (float)region.height);

	int max_points = 1 + 3 * n;
	OvPlacedRect *placed = calloc(n, sizeof(OvPlacedRect));
	OvPoint *cands = calloc(max_points, sizeof(OvPoint));
	OvPoint *feas = calloc(max_points, sizeof(OvPoint));

	if (!placed || !cands || !feas) {
		free(items);
		free(placed);
		free(cands);
		free(feas);
		return;
	}

	float low = 0.0f, high = 1.0f, best_s = 0.0f;
	for (int iter = 0; iter < 50; iter++) {
		float mid = (low + high) / 2.0f;
		bool ok = true;
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * mid;
			float h = items[k].orig_h * mid;
			OvPlacedRect out;
			if (!try_place(placed, placed_cnt, w, h, (float)gap_inner,
						   max_avail_w, max_avail_h, &out, cands, feas)) {
				ok = false;
				break;
			}
			placed[placed_cnt++] = out;
		}

		if (ok) {
			best_s = mid;
			low = mid;
		} else {
			high = mid;
		}
	}

	if (best_s > 0.0f) {
		int placed_cnt = 0;

		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			OvPlacedRect out;
			try_place(placed, placed_cnt, w, h, (float)gap_inner, max_avail_w,
					  max_avail_h, &out, cands, feas);
			placed[placed_cnt++] = out;
		}

		center_placed_rows(placed, n, (float)gap_inner);

		float box_w = 0, box_h = 0;
		for (int k = 0; k < n; k++) {
			float r = placed[k].x + placed[k].w;
			float b = placed[k].y + placed[k].h;
			if (r > box_w)
				box_w = r;
			if (b > box_h)
				box_h = b;
		}

		float dx = (max_avail_w - box_w) / 2.0f;
		float dy = (max_avail_h - box_h) / 2.0f;
		float base_x = region.x + dx;
		float base_y = region.y + dy;

		// Collects the target geometry of all clients and calls
		// client_tile_resize once at the end.
		struct wlr_box *overview_boxes = calloc(n, sizeof(*overview_boxes));
		if (!overview_boxes) {
			free(items);
			free(placed);
			free(cands);
			free(feas);
			return;
		}
		for (int k = 0; k < n; k++) {
			float w = items[k].orig_w * best_s;
			float h = items[k].orig_h * best_s;
			int ix = (int)(base_x + placed[k].x + 0.5f);
			int iy = (int)(base_y + placed[k].y + 0.5f);
			int iw = (int)(ix + w + 0.5f) - ix;
			int ih = (int)(iy + h + 0.5f) - iy;
			overview_boxes[k] = (struct wlr_box){ix, iy, iw, ih};
		}

		for (int k = 0; k < n; k++) {
			client_tile_resize(items[k].c, overview_boxes[k], 0);
		}
		free(overview_boxes);
	}

	free(items);
	free(placed);
	free(cands);
	free(feas);
}

void overview_scale(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	int orig_n = m->visible_clients;
	if (orig_n == 0)
		return;

	Client **client_list = calloc(orig_n, sizeof(Client *));
	if (!client_list)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			client_list[n++] = c;
		}
	}

	if (n > 0) {
		struct wlr_box region = {
			.x = (int)(m->w.x + target_gappo),
			.y = (int)(m->w.y + target_gappo),
			.width = (int)fmaxf(1.0f, m->w.width - 2 * target_gappo),
			.height = (int)fmaxf(1.0f, m->w.height - 2 * target_gappo),
		};
		overview_pack_region(client_list, n, region, target_gappi);
	}

	free(client_list);
}

// Computes the region box for grid slot i (row-major, i/cols row, i%cols
// column) within outer, using compute_grid_dims's cols/rows/overcols --
// same equal-width-column/equal-height-row distribution grid() itself uses
// for uniform (unweighted) items, with the trailing short row (if any)
// horizontally centered rather than left-aligned, matching grid().
static struct wlr_box grid_slot_box(struct wlr_box outer, int32_t cols,
									int32_t rows, int32_t overcols, int32_t i,
									int32_t gap) {
	int32_t row_idx = i / cols;
	int32_t col_idx = i % cols;
	int32_t items_in_row =
		(overcols > 0 && row_idx == rows - 1) ? overcols : cols;

	float avail_w = fmaxf(1.0f, outer.width - (float)(cols - 1) * gap);
	float avail_h = fmaxf(1.0f, outer.height - (float)(rows - 1) * gap);
	float col_w = avail_w / cols;
	float row_h = avail_h / rows;

	float row_w = items_in_row * col_w + (items_in_row - 1) * gap;
	float row_x0 = outer.x + (outer.width - row_w) / 2.0f;

	return (struct wlr_box){
		.x = (int)(row_x0 + col_idx * (col_w + gap) + 0.5f),
		.y = (int)(outer.y + row_idx * (row_h + gap) + 0.5f),
		.width = (int)(col_w + 0.5f),
		.height = (int)(row_h + 0.5f),
	};
}

// Tag-grouped overview: partitions the overview area into one region per
// occupied tag, grid-of-rows style (compute_grid_dims -- the same
// cols=ceil(sqrt(n)) row/column split grid() uses for windows, applied to
// tag regions instead). Each region then shows that TAG's own real,
// already-configured layout (scroller stays a scroller, dwindle stays a
// dwindle tree, ...): m->w and the monitor's current-tag state are
// temporarily redirected to the region box and that tag, the tag's own
// Layout->arrange(m) is called completely unmodified, then everything is
// restored. See DESIGN.md for why this is safe (client_tile_resize skips
// the real protocol resize event entirely while m->isoverview is true; the
// only_calculate pre_calculate_before_arrange refresh is required because
// grid()/dwindle() trust monitor-global visible_*_tiling_clients counters
// rather than recomputing them).
void overview_scale_grouped(Monitor *m) {
	if (m->visible_clients == 0)
		return;

	int32_t count_by_tag[tag_num_MAX + 1] = {0};
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c))
			count_by_tag[get_client_tag_idx(c)]++;
	}

	int32_t occupied_tags[tag_num_MAX + 1];
	int32_t occupied_count = 0;
	for (int32_t t = 0; t <= tag_num_MAX; t++)
		if (count_by_tag[t] > 0)
			occupied_tags[occupied_count++] = t;

	if (occupied_count == 0)
		return;

	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	struct wlr_box outer = {
		.x = (int)(m->w.x + target_gappo),
		.y = (int)(m->w.y + target_gappo),
		.width = (int)fmaxf(1.0f, m->w.width - 2 * target_gappo),
		.height = (int)fmaxf(1.0f, m->w.height - 2 * target_gappo),
	};

	int32_t cols, rows, overcols;
	compute_grid_dims(occupied_count, &cols, &rows, &overcols);

	struct wlr_box real_w = m->w;
	uint32_t real_tagset = m->tagset[m->seltags];
	uint32_t real_curtag = m->pertag->curtag;
	// scroller() otherwise anchors its non-centered scroll positioning to
	// the root client's stale geom.x from its last real (full-width)
	// render, which is meaningless once m->w is a small region box --
	// forcing centered mode (the same path scroller() already takes when
	// the user sets this) sidesteps that stale-position dependency
	// entirely, reusing scroller()'s own existing centering behavior
	// rather than reimplementing anything.
	int32_t real_scroller_focus_center = config.scroller_focus_center;
	config.scroller_focus_center = 1;

	for (int32_t i = 0; i < occupied_count; i++) {
		// get_client_tag_idx()'s return value IS the 1-based tag number --
		// the same convention pertag->curtag/ltidxs[] already use natively
		// (confirmed against every other call site in the codebase, e.g.
		// animation/client.c:777, manage/client.c:1148). The tagset
		// bitmask is the one place that stays 0-based (parse_tag_mask()
		// does `1 << (num - 1)`), so only that conversion needs a -1.
		int32_t t = occupied_tags[i];
		struct wlr_box region =
			grid_slot_box(outer, cols, rows, overcols, i, target_gappi);

		m->w = region;
		m->tagset[m->seltags] = 1U << (t - 1);
		m->pertag->curtag = (uint32_t)t;
		pre_calculate_before_arrange(m, false, false, true);
		m->pertag->ltidxs[t]->arrange(m);
	}

	m->w = real_w;
	m->tagset[m->seltags] = real_tagset;
	m->pertag->curtag = real_curtag;
	config.scroller_focus_center = real_scroller_focus_center;
	pre_calculate_before_arrange(m, false, false, true);
}

// Overview layout: focused window centered (about half screen width), remaining
// windows split on both sides.
void overview_layout_column(Monitor *m, Client **items, int cnt, float x,
							float top, float col_w, float col_h, float gap) {
	if (cnt <= 0)
		return;

	float *ws = calloc(cnt, sizeof(float));
	float *hs = calloc(cnt, sizeof(float));
	if (!ws || !hs) {
		free(ws);
		free(hs);
		return;
	}

	// Width fills the column width; height is proportional.
	float total_h = 0.0f;
	for (int i = 0; i < cnt; i++) {
		float ow = items[i]->overview_backup_geom.width;
		float oh = items[i]->overview_backup_geom.height;
		if (ow <= 0 || oh <= 0) {
			ow = 100.0f;
			oh = 100.0f;
		}
		ws[i] = col_w;
		hs[i] = col_w * (oh / ow);
		total_h += hs[i];
	}

	// Scales the whole item down when it is too tall.
	float gap_total = gap * (cnt - 1);
	if (total_h + gap_total > col_h) {
		float s = (col_h - gap_total) / total_h;
		if (s < 0.0f)
			s = 0.01f;
		for (int i = 0; i < cnt; i++) {
			ws[i] *= s;
			hs[i] *= s;
		}
		total_h *= s;
	}

	// Vertically centers when there is extra room.
	float y = top;
	if (total_h + gap_total < col_h)
		y = top + (col_h - (total_h + gap_total)) / 2.0f;

	for (int i = 0; i < cnt; i++) {
		int ix = (int)(x + (col_w - ws[i]) / 2.0f + 0.5f);
		int iy = (int)(y + 0.5f);
		client_tile_resize(items[i],
						   (struct wlr_box){ix, iy, (int)ws[i], (int)hs[i]}, 0);
		y += hs[i] + gap;
	}

	free(ws);
	free(hs);
}

void overview_scale_tab(Monitor *m) {
	int32_t target_gappo = config.overviewgappo;
	int32_t target_gappi = config.overviewgappi;

	if (m->visible_clients <= 0)
		return;

	Client **items = calloc(m->visible_clients, sizeof(Client *));
	if (!items)
		return;

	int n = 0;
	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon != m)
			continue;
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			items[n++] = c;
		}
	}

	if (n == 0) {
		free(items);
		return;
	}

	// Index of the focused window (or the first one if none).
	Client *sel = m->sel;
	int focus_idx = 0;
	for (int i = 0; i < n; i++) {
		if (items[i] == sel) {
			focus_idx = i;
			break;
		}
	}

	// Uses the larger gap between columns and the smaller gap at the edges.
	float gap_mid = (float)target_gappo;
	float gap_edge = (float)target_gappi;
	if (gap_mid < gap_edge) {
		float tmp = gap_mid;
		gap_mid = gap_edge;
		gap_edge = tmp;
	}
	gap_mid *= 0.5f; /* Halves the gap between columns. */

	float avail_w = fmaxf(1.0f, m->w.width - 2 * gap_edge);
	float avail_h = fmaxf(1.0f, m->w.height - 2 * gap_edge);

	// Center column ratio is configurable; both sides split the remaining space
	// evenly.
	float center_w = avail_w * config.overcircle_center_ratio;
	float side_w = (avail_w - center_w - 2.0f * gap_mid) * 0.5f;
	if (side_w < 1.0f)
		side_w = 1.0f;

	float base_x = m->w.x + gap_edge;
	float base_y = m->w.y + gap_edge;

	float left_x = base_x;
	float center_x = base_x + side_w + gap_mid;
	float right_x = center_x + center_w + gap_mid;

	// The focused window is centered.
	Client *focus = items[focus_idx];
	{
		float ow = focus->overview_backup_geom.width;
		float oh = focus->overview_backup_geom.height;
		if (ow <= 0 || oh <= 0) {
			ow = 100.0f;
			oh = 100.0f;
		}
		float w = center_w;
		float h = w * (oh / ow);
		if (h > avail_h) {
			float s = avail_h / h;
			w *= s;
			h *= s;
		}
		int ix = (int)(center_x + (center_w - w) / 2.0f + 0.5f);
		int iy = (int)(base_y + (avail_h - h) / 2.0f + 0.5f);
		client_tile_resize(focus, (struct wlr_box){ix, iy, (int)w, (int)h}, 0);
	}

	// The rest split into the left/right columns; on focus change they
	// circulate (rotate).
	Client **left = calloc(n, sizeof(Client *));
	Client **right = calloc(n, sizeof(Client *));
	if (!left || !right) {
		free(items);
		free(left);
		free(right);
		return;
	}
	int rest = n - 1;
	int right_cnt = (rest + 1) / 2;
	int left_cnt = rest - right_cnt;

	int nr = 0;
	for (int k = 0; k < right_cnt; k++) {
		int idx = (focus_idx + 1 + k) % n;
		right[nr++] = items[idx];
	}
	int nl = 0;
	for (int k = 0; k < left_cnt; k++) {
		int idx = (focus_idx - 1 - k + n) % n;
		left[nl++] = items[idx];
	}

	overview_layout_column(m, left, nl, left_x, base_y, side_w, avail_h,
						   gap_edge);
	overview_layout_column(m, right, nr, right_x, base_y, side_w, avail_h,
						   gap_edge);

	free(items);
	free(left);
	free(right);
}

void create_jump_hints(Monitor *m) {
	// Uses the static default sequence when jump_labels is not configured.
	const char *jump_labels =
		config.jump_labels ? config.jump_labels : default_jump_labels;
	if (!jump_labels || !jump_labels[0])
		return;
	size_t jump_labels_len = strlen(jump_labels);
	int label_idx = 0;
	Client *c;

	wl_list_for_each(c, &server.clients, link) {
		if (VISIBLEON(c, m) && !c->isunglobal && !client_is_x11_popup(c)) {
			if (label_idx >= (int)jump_labels_len)
				break;
			char c_char = jump_labels[label_idx];
			c->jump_char = c_char;

			char label_text[2] = {c_char, '\0'};
			if (!c->jump_label_node)
				continue;
			mango_jump_label_node_update(c->jump_label_node, label_text,
										 m->wlr_output->scale);
			overview_update_jump_label(c);
			label_idx++;
		}
	}
}

void finish_jump_mode(Monitor *m) {
	if (!m->is_jump_mode)
		return;

	Client *c;
	wl_list_for_each(c, &server.clients, link) {
		if (c->mon == m) {
			if (c->jump_label_node &&
				c->jump_label_node->scene_buffer->node.enabled) {
				c->jump_char = '\0';
				wlr_scene_node_set_enabled(
					&c->jump_label_node->scene_buffer->node, false);
			}
		}
	}
	m->is_jump_mode = 0;
}

void overview(Monitor *m) {
	if (m->ov_tab_layout && !m->is_jump_mode && !m->ov_normal_mode) {
		overview_scale_tab(m);
	} else if (config.overview_group_by_tag) {
		overview_scale_grouped(m);
	} else {
		overview_scale(m);
	}

	if (m->is_jump_mode) {
		create_jump_hints(m);
	}
}
