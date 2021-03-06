/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NVDLA_I_LOADABLE_H
#define NVDLA_I_LOADABLE_H

#include <string>
#include <vector>

#include "nvdla/c/NvDlaType.h"
#include "nvdla/c/NvDlaLoadable.h"

#include "nvdla/IType.h"



// some gnu stuff is defining these in host mode... evil.
#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

//
// ILoadable synopsis
//
//     storage/manipulation of
//         . memory object w/content
//         . blank memory object
//         . content w/engine rev check
//
//     task submission model
//         two engine types: dla and cpu
//         describe then submit a set (small ish?) of tasks
//         global
//             memory objects -> placed in address list
//             event objects  -> placed in event lists
//             io objects     -> tensor bind point list ?? actually needed ??
//         per-task
//             engine type
//             address list content (addr0, any ref'd cmd buffers, ++)
//             preaction event list
//             postaction event list
//
//
//     task set load (all at once)
//         global setup operations
//             resolve alloc'd memory objects
//             resolve alloc'd event objects
//         for each task inspect its address list
//             move mem as needed for content setup
//             mark any still unsatisfied
//         for each task inspect its event list
//             mark any which continue to be unsatisfied
//
//     task set exec (all at once)
//         check for unbound memory
//         check for unbond events
//         submit each task

//

namespace nvdla
{

class ILoadable
{
public:

    enum Interface {
        Interface_NONE = NVDLA_LOADABLE_INTERFACE_NONE,
        Interface_DLA1 = NVDLA_LOADABLE_INTERFACE_DLA1,
    };

    enum MemoryDomain {
        MemoryDomain_SYSMEM = NVDLA_LOADABLE_MEMORY_DOMAIN_SYSMEM,
        MemoryDomain_SRAM = NVDLA_LOADABLE_MEMORY_DOMAIN_SRAM,
    };

    enum MemoryFlags {
        MemoryFlags_NONE  = NVDLA_LOADABLE_MEMORY_FLAGS_NONE,
        MemoryFlags_ALLOC  = NVDLA_LOADABLE_MEMORY_FLAGS_ALLOC,
        MemoryFlags_SET    = NVDLA_LOADABLE_MEMORY_FLAGS_SET,
        MemoryFlags_INPUT  = NVDLA_LOADABLE_MEMORY_FLAGS_INPUT,
        MemoryFlags_OUTPUT = NVDLA_LOADABLE_MEMORY_FLAGS_OUTPUT,
    };

    enum EventOp {
        EventOp_WAIT   = NVDLA_LOADABLE_EVENT_OP_WAIT,
        EventOp_SIGNAL = NVDLA_LOADABLE_EVENT_OP_SIGNAL
    };

    struct Version
    {
        NvU8 major;
        NvU8 minor;
        NvU8 sub_minor;
        Version(NvU8 maj, NvU8 min, NvU8 sub) : major(maj), minor(min), sub_minor(sub) { }
        Version() : major(0), minor(0), sub_minor(0) { }

        void toC(NvDlaLoadableVersion &c) const
        {
            c.major = major;
            c.minor = minor;
            c.subMinor = sub_minor;
        }
    };

    struct MemoryListEntry
    {
        NvU16 id;
        NvU64 size;
        NvU32 alignment; // 0 for n/a, otherwise byte alignment
        NvU8  domain;
        static inline NvU8 domain_sysmem() { return MemoryDomain_SYSMEM; }
        NvU8  flags; // alloc or alloc_content or is-input or is-output
        static inline NvU8  flags_alloc()  { return MemoryFlags_ALLOC;  }
        static inline NvU8  flags_set()    { return MemoryFlags_SET;    }
        static inline NvU8  flags_input()  { return MemoryFlags_INPUT;  }
        static inline NvU8  flags_output() { return MemoryFlags_OUTPUT; }
        NvU16 bind_id;  // valid iff flag_{input|output}()  is set
        NvU16 tensor_desc_id; // valid iff bind_id is valid ( != -1 )
        std::vector<std::string> contents;  // symbolic reference to content blob
        std::vector<uint64_t>    offsets;   // associated offset for contents

        MemoryListEntry() : id(0), size(0), alignment(0), domain(0), flags(0),
                            bind_id(0), tensor_desc_id(0), contents(), offsets() { }
        MemoryListEntry(const MemoryListEntry &o) : id(o.id), size(o.size), alignment(o.alignment), domain(o.domain), flags(o.flags),
                                                    bind_id(o.bind_id),
                                                    tensor_desc_id(o.tensor_desc_id),
                                                    contents(o.contents),
                                                    offsets(o.offsets) { }
    };

    struct EventListEntry
    {
        NvU16 id;
        NvU16 target;
        NvU8 op;
        static inline NvU8 op_wait() { return EventOp_WAIT; }
        static inline NvU8 op_signal() { return EventOp_SIGNAL; }
        NvU32 val;
        void toC(NvDlaLoadableEventListEntry &c) const
        {
            c.id = id;
            c.target = target;
            c.op = op;
            c.val = val;
        }
    };

    struct TaskListEntry
    {
        NvU16 id;
        NvU32 interface; // DLA interface id
        static inline NvU32 interface_NONE() { return Interface_NONE; }
        static inline NvU32 interface_DLA1() { return Interface_DLA1; }

        NvS16 instance; // -1 := for any available
        static inline NvS16 instance_ANY() { return -1; }

        std::vector<NvU16> preactions;   // [event id]...
        std::vector<NvU16> postactions;  // [event id]...
        std::vector<NvU16> address_list; // [addr list id]...[addr list id]
    };

    struct SubmitListEntry
    {
        NvU16 id;
        std::vector<NvU16> tasks;
    };

    struct AddressListEntry
    {
        NvU16 id;     // all possible address list entries are given an id
        NvU16 mem_id; // determines hRm (+offset from below)
        NvU64 size;   // assert size <= memory[mem_id].size
        NvU64 offset; // assert (offset + size) <= memory[mem_id].size
        AddressListEntry() : id(0), mem_id(0), size(0), offset(0) { }
        void toC(NvDlaLoadableAddressListEntry &c) const {
            c.id = id;
            c.memId = mem_id;
            c.size = size;
            c.offset = offset;
        }
    };


    struct TensorDescListEntry
    {
        NvU16 id;
        NvU16 mem_id;
        NvU64 size;
        NvU64 offset;

        Dims4 dims;
        DataFormat::UnderlyingType data_format;
        DataType::UnderlyingType   data_type;
        DataCategory::UnderlyingType data_category;
        PixelFormat::UnderlyingType  pixel_format;
        PixelMapping::UnderlyingType pixel_mapping;

        NvU32 line_stride;
        NvU32 surf_stride;
        NvU32 plane_stride;

        void fromC(const NvDlaLoadableTensorDescListEntry &c)
        {
            id = c.id;
            mem_id = c.mem_id;
            size   = c.size;
            offset = c.offset;
            dims.n = c.dims.n;
            dims.c = c.dims.c;
            dims.h = c.dims.h;
            dims.w = c.dims.w;
            data_format = c.dataFormat;
            data_type = c.dataType;
            data_category = c.dataCategory;
            pixel_format = c.pixelFormat;
            pixel_mapping = c.pixelMapping;
            line_stride = c.lineStride;
            surf_stride = c.surfStride;
            plane_stride = c.planeStride;
        }

        void toC(NvDlaLoadableTensorDescListEntry &c) const
        {
            c.id = id;
            c.mem_id = id;
            c.size   = size;
            c.offset = offset;
            c.dims.n = dims.n;
            c.dims.c = dims.c;
            c.dims.h = dims.h;
            c.dims.w = dims.w;
            c.dataFormat = data_format;
            c.dataType = data_type;
            c.dataCategory = data_category;
            c.pixelFormat = pixel_format;
            c.pixelMapping = pixel_mapping;
            c.lineStride = line_stride;
            c.surfStride = surf_stride;
            c.planeStride = plane_stride;
        }
    };

    struct Blob
    {
        std::string name;
        NvU64 size;
        Interface interface;
        Version version;
    };

    virtual std::string getName() const = 0;

    virtual int getNumMemoryListEntries() const = 0;
    virtual MemoryListEntry getMemoryListEntry(NvU16 mem_id) const = 0;

    virtual int getNumEventListEntries() const = 0;
    virtual EventListEntry getEventListEntry(NvU16 event_id) const = 0;

    virtual int getNumTaskListEntries() const = 0;
    virtual TaskListEntry getTaskListEntry(NvU16 task_id) const = 0;

    virtual int getNumAddressListEntries() const = 0;
    virtual AddressListEntry getAddressListEntry(NvU16 i) const = 0;

    virtual int getNumTensorDescListEntries() const = 0;
    virtual TensorDescListEntry getTensorDescListEntry(NvU16 i) const = 0;

    virtual NvDlaError getNetworkDataType(DataType::UnderlyingType *) const = 0;

    virtual NvDlaError getNumInputTensors(int *) const = 0;
    virtual NvDlaError getInputTensorDesc(NvU16 id, ILoadable::TensorDescListEntry *) const = 0;

    virtual NvDlaError getNumOutputTensors(int *) const = 0;
    virtual NvDlaError getOutputTensorDesc(NvU16 id, ILoadable::TensorDescListEntry *) const = 0;

protected:
    ILoadable();
    virtual ~ILoadable();
};

} // nvdla
#endif // NVDLA_I_LOADABLE_H
