// SPDX-License-Identifier: Apache-2.0
#define __STDC_WANT_LIB_EXT1__ 1
#include "myers.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <stddef.h>

#ifndef ARRAY_MARKER
#define ARRAY_MARKER "_t"
#endif
#ifndef ARRAY_MARKER_VALUE
#define ARRAY_MARKER_VALUE "a"
#endif

struct seg { int type; int a_start; int b_start; int len; };

static int ensure_seg_capacity(struct seg **segs, int *cap, int need)
{
    if (*cap >= need) return 1;
    int nc = *cap ? *cap : 16;
    while (nc < need) {
        if (nc > INT_MAX / 2) return 0;
        nc *= 2;
    }
    struct seg *ns = (struct seg *)realloc(*segs, (size_t)nc * sizeof(**segs));
    if (!ns) return 0;
    *segs = ns; *cap = nc; return 1;
}

/* Merge add+del pairs of objects into nested diffs (jsondiffpatch arrays-of-objects rule) */
static void transform_array_object_changes(cJSON *diff_obj,
                                           const struct json_diff_options *opts)
{
    if (!diff_obj) return;
    int cap = 8, count = 0;
    int *idxs = (int *)malloc((size_t)cap * sizeof(int));
    if (!idxs) return;
    for (cJSON *ch = diff_obj->child; ch; ch = ch->next) {
        const char *k = ch->string;
        if (!k || k[0] == '_' || strcmp(k, ARRAY_MARKER) == 0)
            continue;
        if (!cJSON_IsArray(ch) || cJSON_GetArraySize(ch) != 1)
            continue;
        cJSON *v0 = cJSON_GetArrayItem(ch, 0);
        if (!v0 || !cJSON_IsObject(v0))
            continue;
        char *endptr = NULL;
        long idx = strtol(k, &endptr, 10);
        if (endptr == k || *endptr != '\0' || idx < 0 || idx > INT_MAX)
            continue;
        if (count == cap) {
            int newcap = cap * 2;
            int *tmp = (int *)realloc(idxs, (size_t)newcap * sizeof(int));
            if (!tmp) { free(idxs); return; }
            idxs = tmp; cap = newcap;
        }
        idxs[count++] = (int)idx;
    }

    char keybuf[32], delbuf[32];
    for (int i = 0; i < count; i++) {
#ifdef __STDC_LIB_EXT1__
        snprintf_s(keybuf, sizeof(keybuf), "%d", idxs[i]);
        snprintf_s(delbuf, sizeof(delbuf), "_%d", idxs[i]);
#else
        snprintf(keybuf, sizeof(keybuf), "%d", idxs[i]);
        snprintf(delbuf, sizeof(delbuf), "_%d", idxs[i]);
#endif
        cJSON *add = cJSON_GetObjectItem(diff_obj, keybuf);
        cJSON *del = cJSON_GetObjectItem(diff_obj, delbuf);
        if (!add || !del || !cJSON_IsArray(add) || cJSON_GetArraySize(add) != 1)
            continue;
        cJSON *new_obj = cJSON_GetArrayItem(add, 0);
        if (!new_obj || !cJSON_IsObject(new_obj)) continue;
        if (!cJSON_IsArray(del) || cJSON_GetArraySize(del) != 3) continue;
        cJSON *old_obj = cJSON_GetArrayItem(del, 0);
        cJSON *z1 = cJSON_GetArrayItem(del, 1);
        cJSON *z2 = cJSON_GetArrayItem(del, 2);
        if (!old_obj || !cJSON_IsObject(old_obj)) continue;
        if (!z1 || !z2 || !cJSON_IsNumber(z1) || !cJSON_IsNumber(z2)) continue;
        if (z1->valuedouble != 0 || z2->valuedouble != 0) continue;

        cJSON *nested = json_diff(old_obj, new_obj, opts);
        cJSON_DeleteItemFromObject(diff_obj, delbuf);
        cJSON_DeleteItemFromObject(diff_obj, keybuf);
        if (nested) {
            cJSON_AddItemToObject(diff_obj, keybuf, nested);
        }
    }
    free(idxs);
}

/* Public SES-based array diff */
cJSON *json_myers_array_diff(const cJSON *left, const cJSON *right,
                             const struct json_diff_options *opts)
{
    int N = cJSON_GetArraySize(left);
    int M = cJSON_GetArraySize(right);
    if (N == M) {
        bool all_equal = true;
        for (int i = 0; i < N; i++) {
            if (!json_value_equal(cJSON_GetArrayItem(left, i), cJSON_GetArrayItem(right, i), opts->strict_equality)) {
                all_equal = false; break;
            }
        }
        if (all_equal) return NULL;
    }

    cJSON **A = (cJSON **)malloc((size_t)N * sizeof(cJSON *));
    cJSON **B = (cJSON **)malloc((size_t)M * sizeof(cJSON *));
    if ((N && !A) || (M && !B)) { free(A); free(B); return NULL; }
    for (int i=0;i<N;i++) A[i]=cJSON_GetArrayItem(left,i);
    for (int j=0;j<M;j++) B[j]=cJSON_GetArrayItem(right,j);

    int lcp = 0;
    while (lcp < N && lcp < M && json_value_equal(A[lcp], B[lcp], opts->strict_equality)) lcp++;
    int lcs = 0;
    while (lcs < (N - lcp) && lcs < (M - lcp) && json_value_equal(A[N-1-lcs], B[M-1-lcs], opts->strict_equality)) lcs++;

    cJSON **A2 = A + lcp;
    cJSON **B2 = B + lcp;
    int N2 = N - lcp - lcs;
    int M2 = M - lcp - lcs;

    if (N2 == 0 && M2 == 0) { free(A); free(B); return NULL; }
    if (N2 == 0 && M2 > 0) {
        cJSON *diff_obj = cJSON_CreateObject(); if (!diff_obj) { free(A); free(B); return NULL; }
        char keybuf[32]; int count = lcp;
        for (int r = 0; r < M2; r++) {
            snprintf(keybuf, sizeof(keybuf), "%d", count);
            cJSON *ia = create_addition_array(B2[r]);
            if (ia) cJSON_AddItemToObject(diff_obj, keybuf, ia);
            count++;
        }
        transform_array_object_changes(diff_obj, opts);
        cJSON_AddStringToObject(diff_obj, ARRAY_MARKER, ARRAY_MARKER_VALUE);
        free(A); free(B);
        return diff_obj;
    }
    if (M2 == 0 && N2 > 0) {
        cJSON *diff_obj = cJSON_CreateObject(); if (!diff_obj) { free(A); free(B); return NULL; }
        char keybuf[32]; int deletedCount = lcp;
        for (int r = 0; r < N2; r++) {
            snprintf(keybuf, sizeof(keybuf), "_%d", deletedCount);
            cJSON *da = create_deletion_array(A2[r]);
            if (da) cJSON_AddItemToObject(diff_obj, keybuf, da);
            deletedCount++;
        }
        transform_array_object_changes(diff_obj, opts);
        cJSON_AddStringToObject(diff_obj, ARRAY_MARKER, ARRAY_MARKER_VALUE);
        free(A); free(B);
        return diff_obj;
    }

    int max = N2 + M2, off = max, vlen = 2*max+1;
    int *V = (int *)calloc((size_t)vlen, sizeof(int));
    if (!V) { free(A); free(B); return NULL; }
    int **trace = (int **)malloc((size_t)(max+1) * sizeof(int*));
    if (!trace) { free(V); free(A); free(B); return NULL; }
    for (int d=0; d<=max; d++) trace[d]=NULL;

    int D_found = -1;
    for (int d=0; d<=max; d++) {
        int *Vd = (int *)malloc((size_t)vlen*sizeof(int)); if(!Vd){ D_found=-1; break; }
        for (int t=0;t<vlen;t++) Vd[t]=V[t];
        trace[d]=Vd;
        for (int k=-d; k<=d; k+=2) {
            int x;
            if (k==-d || (k!=d && V[k-1+off] < V[k+1+off])) x = V[k+1+off]; else x = V[k-1+off]+1;
            int y = x - k;
            while (x < N2 && y < M2 && json_value_equal(A2[x], B2[y], opts->strict_equality)) { x++; y++; }
            V[k+off]=x;
            if (x>=N2 && y>=M2) { D_found=d; break; }
        }
        if (D_found!=-1) break;
    }

    enum { SEG_EQUAL=0, SEG_INS=1, SEG_DEL=2 };
    struct seg *segs=NULL; int segs_count=0, segs_cap=0;
    int x=N2, y=M2;
    for (int d=D_found; d>0; d--) {
        int *Vprev = trace[d-1];
        int k = x - y;
        int prev_k = (k == -d || (k != d && Vprev[k-1+off] < Vprev[k+1+off])) ? k+1 : k-1;
        int x_prev = Vprev[prev_k+off];
        int y_prev = x_prev - prev_k;
        int x_mid = x_prev, y_mid = y_prev;
        int seg_type, a_start=0,b_start=0,len=1;
        if (prev_k == k+1) { seg_type=SEG_DEL; a_start=x_prev; b_start=y_prev; x_mid=x_prev+1; y_mid=y_prev; }
        else { seg_type=SEG_INS; a_start=x_prev; b_start=y_prev; x_mid=x_prev; y_mid=y_prev+1; }
        int eq_len = x - x_mid;
        if (eq_len>0) { if (!ensure_seg_capacity(&segs,&segs_cap,segs_count+1)) { /* OOM */ } else { segs[segs_count++] = (struct seg){SEG_EQUAL,x_mid,y_mid,eq_len}; } }
        if (!ensure_seg_capacity(&segs,&segs_cap,segs_count+1)) { /* OOM */ } else { segs[segs_count++] = (struct seg){seg_type,a_start,b_start,len}; }
        x = x_prev; y = y_prev;
    }
    for (int l=0,r=segs_count-1; l<r; l++,r--){ struct seg tmp=segs[l]; segs[l]=segs[r]; segs[r]=tmp; }

    cJSON *diff_obj = cJSON_CreateObject();
    if (!diff_obj){ for(int d=0; d<=max; d++) free(trace[d]); free(trace); free(V); free(A); free(B); free(segs); return NULL; }
    int count = lcp, deletedCount = lcp; char keybuf[32];
    int ia_ptr = 0, ib_ptr = 0;
    int si = 0;
    while (si < segs_count) {
        struct seg s = segs[si];
        if (s.type == SEG_EQUAL) { si++; continue; }
        if (s.type == SEG_DEL) {
            for (int r = 0; r < s.len; r++) {
                snprintf(keybuf, sizeof(keybuf), "_%d", deletedCount);
                if (ia_ptr < N2) {
                    cJSON *da = create_deletion_array(A2[ia_ptr]);
                    if (da) cJSON_AddItemToObject(diff_obj, keybuf, da);
                    ia_ptr++;
                }
                deletedCount++;
            }
            si++;
            continue;
        }
        if (s.type == SEG_INS) {
            for (int r = 0; r < s.len; r++) {
                snprintf(keybuf, sizeof(keybuf), "%d", count);
                if (ib_ptr < M2) {
                    cJSON *iaa = create_addition_array(B2[ib_ptr]);
                    if (iaa) cJSON_AddItemToObject(diff_obj, keybuf, iaa);
                    ib_ptr++;
                }
                count++;
            }
            si++;
            continue;
        }
    }
    while (ia_ptr < N2) { snprintf(keybuf, sizeof(keybuf), "_%d", deletedCount); cJSON *da = create_deletion_array(A2[ia_ptr]); if (da) cJSON_AddItemToObject(diff_obj, keybuf, da); ia_ptr++; deletedCount++; }
    while (ib_ptr < M2) { snprintf(keybuf, sizeof(keybuf), "%d", count); cJSON *iaa = create_addition_array(B2[ib_ptr]); if (iaa) cJSON_AddItemToObject(diff_obj, keybuf, iaa); ib_ptr++; count++; }

    for (int d = 0; d <= max; d++) free(trace[d]);
    free(trace); free(V); free(A); free(B); free(segs);

    transform_array_object_changes(diff_obj, opts);
    bool non_marker = false; for (cJSON *it = diff_obj->child; it; it = it->next) { if (strcmp(it->string, ARRAY_MARKER) != 0) { non_marker = true; break; } }
    if (non_marker) { cJSON_AddStringToObject(diff_obj, ARRAY_MARKER, ARRAY_MARKER_VALUE); return diff_obj; }
    cJSON_Delete(diff_obj); return NULL;
}
