/*
 * drivers/staging/android/ion/ion_cma_heap.c
 *
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/device.h>
#include <linux/ion.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/exynos_ion.h>
#include <linux/dma-contiguous.h>

/* for ion_heap_ops structure */
#include "ion_priv.h"

struct ion_cma_heap {
	struct ion_heap heap;
	struct device *dev;
};

#define to_cma_heap(x) container_of(x, struct ion_cma_heap, heap)

/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len, unsigned long align,
			    unsigned long flags)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);
	struct device *dev = cma_heap->dev;
	struct ion_buffer_info *info;
	struct page *page;
	unsigned long size = len;
	int ret;

	dev_dbg(dev, "Request buffer allocation len %ld\n", len);

	if (!ion_is_heap_available(heap, flags, NULL))
		return -EPERM;

	info = kzalloc(sizeof(struct ion_buffer_info), GFP_KERNEL);
	if (!info) {
		dev_err(dev, "Can't allocate buffer info\n");
		return -ENOMEM;
	}

	if (buffer->flags & ION_FLAG_PROTECTED) {
		align = ION_PROTECTED_BUF_ALIGN;
		size = ALIGN(len, ION_PROTECTED_BUF_ALIGN);
	}

	page = dma_alloc_from_contiguous(dev,
			(PAGE_ALIGN(size) >> PAGE_SHIFT),
			(align ? get_order(align) : 0));
	if (!page) {
		ret = -ENOMEM;
		dev_err(dev, "Fail to allocate buffer\n");
		goto err;
	}

	info->handle = phys_to_dma(dev, page_to_phys(page));
	info->cpu_addr = page_address(page);
	memset(info->cpu_addr, 0, len);

	ret = dma_get_sgtable(dev, &info->table, info->cpu_addr, info->handle,
			len);
	if (ret)
		goto free_mem;

	if (!ion_buffer_cached(buffer) && !(buffer->flags & ION_FLAG_PROTECTED)) {
		if (ion_buffer_need_flush_all(buffer))
			flush_all_cpu_caches();
		else
			__flush_dcache_area(page_address(sg_page(info->table.sgl)),
									len);
	}

	/* keep this for memory release */
	buffer->priv_virt = info;

	if (buffer->flags & ION_FLAG_PROTECTED) {
		info->prot_desc.chunk_count = 1;
		info->prot_desc.flags = heap->id;
		info->prot_desc.chunk_size = size;
		info->prot_desc.bus_address = info->handle;
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		ret = ion_secure_protect(buffer);
		if (ret) {
			pr_err("%s: Failed to protect buffer of %zu bytes\n",
			       __func__, size);
			goto err_protect;
		}
#endif
	}

	dev_dbg(dev, "Allocate buffer %p\n", buffer);
	return 0;
err_protect:
	sg_free_table(&info->table);
free_mem:
	dma_release_from_contiguous(dev, page,
			(PAGE_ALIGN(size) >> PAGE_SHIFT));
err:
	kfree(info);
	ion_debug_heap_usage_show(heap);
	return ret;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	struct device *dev = cma_heap->dev;
	struct ion_buffer_info *info = buffer->priv_virt;
	unsigned long size = buffer->size;

	dev_dbg(dev, "Release buffer %p\n", buffer);

	if (buffer->flags & ION_FLAG_PROTECTED) {
		ion_secure_unprotect(buffer);
		size = ALIGN(size, ION_PROTECTED_BUF_ALIGN);
	}

	/* release memory */
	dma_release_from_contiguous(dev,
			phys_to_page(dma_to_phys(dev, info->handle)),
			(PAGE_ALIGN(size) >> PAGE_SHIFT));

	/* release sg table */
	sg_free_table(&info->table);
	kfree(info);
}

/* return physical address in addr */
static int ion_cma_phys(struct ion_heap *heap, struct ion_buffer *buffer,
			ion_phys_addr_t *addr, size_t *len)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	struct device *dev = cma_heap->dev;
	struct ion_buffer_info *info = buffer->priv_virt;

	dev_dbg(dev, "Return buffer %p physical address %pa\n", buffer,
		&info->handle);

	*addr = info->handle;
	*len = buffer->size;

	return 0;
}

static struct sg_table *ion_cma_heap_map_dma(struct ion_heap *heap,
					     struct ion_buffer *buffer)
{
	struct ion_buffer_info *info = buffer->priv_virt;

	return &info->table;
}

static void ion_cma_heap_unmap_dma(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
}

static int ion_cma_mmap(struct ion_heap *mapper, struct ion_buffer *buffer,
			struct vm_area_struct *vma)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	struct device *dev = cma_heap->dev;
	struct ion_buffer_info *info = buffer->priv_virt;

	return dma_mmap_coherent(dev, vma, info->cpu_addr, info->handle,
				 buffer->size);
}

static void *ion_cma_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	struct ion_buffer_info *info = buffer->priv_virt;
	/* kernel memory mapping has been done at allocation time */
	return info->cpu_addr;
}

static void ion_cma_unmap_kernel(struct ion_heap *heap,
					struct ion_buffer *buffer)
{
}

static struct ion_heap_ops ion_cma_ops = {
	.allocate = ion_cma_allocate,
	.free = ion_cma_free,
	.map_dma = ion_cma_heap_map_dma,
	.unmap_dma = ion_cma_heap_unmap_dma,
	.phys = ion_cma_phys,
	.map_user = ion_cma_mmap,
	.map_kernel = ion_cma_map_kernel,
	.unmap_kernel = ion_cma_unmap_kernel,
};

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_heap *cma_heap;

	cma_heap = kzalloc(sizeof(struct ion_cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return ERR_PTR(-ENOMEM);

	dev_set_name(data->priv, data->name);
	cma_heap->heap.ops = &ion_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->dev = data->priv;
	cma_heap->heap.type = ION_HEAP_TYPE_DMA;
	return &cma_heap->heap;
}

void ion_cma_heap_destroy(struct ion_heap *heap)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);

	kfree(cma_heap);
}
