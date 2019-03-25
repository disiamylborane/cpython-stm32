// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <cstring>

volatile uint32_t freemem = 0;

struct alignas(8) mblock {
    uint32_t curr_size_mblock_beats : 28;
    uint32_t processid_1 : 3;
    uint32_t is_occupied : 1;

    uint32_t prev_size_mblock_beats : 28;
    uint32_t processid_0 : 4;

    void set_process_id(uint8_t const process_id){
        processid_0 = process_id & 0b1111;
        processid_1 = (process_id & 0b111) >> 4;
    }
    uint8_t get_process_id(){
        return (processid_1 & 0b111)<<4 | (processid_0 & 0b1111);
    }
};

struct free_mblock {
    mblock base;
    free_mblock* next_free_block_ptr;
    free_mblock* prev_free_block_ptr;
};

static_assert(alignof(mblock) == alignof(free_mblock));

struct RAMRegionDescriptor{
    mblock* const root;
    mblock* const end;
    free_mblock* free_root;
};

RAMRegionDescriptor regions [] = {
    {
        (mblock*)(0xD0000000),
        (mblock*)(0xD0400000),
        (free_mblock*)(0xD0000000),
    }
};

static unsigned char current_processid = 0;

RAMRegionDescriptor& get_memory_region(mblock* address) {
    for(auto &reg: regions) {
        if(reg.root<=address and address<reg.end) {
            return reg;
        }
    }
    assert(0);
}

extern "C"
void init_memory_manager()
{
    auto init_reg = [] (RAMRegionDescriptor& region){
        region.free_root = (free_mblock*)region.root;
        uint32_t reg_size = (region.end-region.root);
        *(free_mblock*)(region.free_root) = {{reg_size,0,0,0,0},0,0};
    };

    for(auto reg: regions)
        init_reg(reg);
}

mblock* get_best_block(RAMRegionDescriptor& region, uint32_t const size_beats)
{
	freemem = 0;
    mblock* curr_block = (mblock*)region.free_root;

    uint32_t best_size_beats = UINT32_MAX;
    mblock* best_block = nullptr;

    while(curr_block){
	    uint32_t curr_size_beats = (curr_block->curr_size_mblock_beats - 1);
		freemem += (curr_size_beats-1)*sizeof(mblock);
        if(curr_size_beats < best_size_beats and curr_size_beats >= size_beats){
            best_size_beats = curr_size_beats;
            best_block = curr_block;
        }
        curr_block = (mblock*)((free_mblock*)curr_block)->next_free_block_ptr;
    }

    return best_block;
}

void* malloc_arrange(RAMRegionDescriptor& region, uint32_t const size_bytes)
{
    constexpr auto calc_size_beats = [](uint32_t size_bytes){
        constexpr uint32_t minsize = sizeof(free_mblock) - sizeof(mblock);
        size_bytes = std::max(minsize, size_bytes);
        uint32_t ret = (size_bytes + (sizeof(mblock)-1))/sizeof(mblock);
        return ret;
    };
    //constexpr auto free_mblock_size_beats = 

    uint32_t const size_beats = calc_size_beats(size_bytes);
    mblock* best_block = get_best_block(region, size_beats);

    if(!best_block)
        return nullptr;

    free_mblock* prev_free_block = ((free_mblock*)best_block)->prev_free_block_ptr;
    free_mblock* next_free_block = ((free_mblock*)best_block)->next_free_block_ptr;

    if( best_block->curr_size_mblock_beats < 1 + size_beats + calc_size_beats(sizeof(free_mblock)) ){
        //Use the whole block
        if(next_free_block){
            next_free_block->prev_free_block_ptr = ((free_mblock*)best_block)->prev_free_block_ptr;
        }
    }
    else {
        //Subdivide block
        if(next_free_block){
            //next_free_block->base.prev_size_mblock_beats = best_block->curr_size_mblock_beats - 1 - size_beats;
            next_free_block->prev_free_block_ptr = (free_mblock*)(best_block + 1 + size_beats);
        }

        next_free_block = (free_mblock*)(best_block + 1 + size_beats);
        *next_free_block = {
            {best_block->curr_size_mblock_beats - 1 - size_beats,
            0, 
            0, 
            1 + size_beats, 
            0},
            ((free_mblock*)best_block)->next_free_block_ptr,
            ((free_mblock*)best_block)->prev_free_block_ptr,
        };

        mblock* next_block = best_block + best_block->curr_size_mblock_beats;
        if(next_block != region.end){
            assert(next_block->prev_size_mblock_beats == best_block->curr_size_mblock_beats);
            next_block->prev_size_mblock_beats = best_block->curr_size_mblock_beats - 1 - size_beats;
        }

        best_block->curr_size_mblock_beats = size_beats+1;
    }

    if(prev_free_block) 
        prev_free_block->next_free_block_ptr = next_free_block;
    else
        region.free_root = next_free_block;

    // Occupy block and return
    best_block->is_occupied = 1;
    best_block->set_process_id(current_processid);
		
    return best_block + 1;
}

extern "C"
void *malloc_special(size_t size){
    return malloc_arrange(regions[0], size);
};

extern "C"
void *malloc(size_t size){
    return malloc_special(size);
};

extern "C"
void *calloc(size_t nmemb, size_t size)
{
	char *r=(char*)malloc(nmemb*size);
	for(char* s = r; s< r + nmemb*size; s++)
		*s = 0;
	return r;
}

extern "C"
void *realloc(void * ptr, size_t size){
	mblock* a = (mblock*)ptr - 1;
	size_t sz = (a->curr_size_mblock_beats - 1)*sizeof(mblock);

  void *newv = malloc_special(size);

	memcpy(newv,ptr,std::min(sz, size));
	return newv;
};

extern "C"
void free(void *__ptr){
	if(!__ptr)
		return;

    mblock* _this_mblock = (mblock*)__ptr - 1;
    auto& region = get_memory_region(_this_mblock);
    free_mblock* this_block = (free_mblock*)_this_mblock;

    // Reconnect blocks
    [&](){

        //Search for the previous free block
        for(mblock* _prev_mblock = _this_mblock; 
                    _prev_mblock > region.root ;)
        {
            _prev_mblock -= _prev_mblock->prev_size_mblock_beats;
            if(!_prev_mblock->is_occupied){
                // if found, reconnect and return
                auto prev_block = (free_mblock*)_prev_mblock;
                auto next_block = prev_block->next_free_block_ptr;

                if(next_block) next_block->prev_free_block_ptr = this_block;
                this_block->prev_free_block_ptr = prev_block;
                this_block->next_free_block_ptr = prev_block->next_free_block_ptr;
                prev_block->next_free_block_ptr = this_block;
                return;
            }
        }
        // if not found, 
        // this_block is the first free block now, connect it to the next
        this_block->prev_free_block_ptr = 0;
        if(region.free_root) region.free_root->prev_free_block_ptr = this_block;
        this_block->next_free_block_ptr = region.free_root;
        region.free_root = this_block;

    }();

    //process_tail:
    this_block->base.is_occupied=0;

    // Now unite blocks if there's free neighbours
    auto unite = [&region](free_mblock* block1, free_mblock* block2){
        block1->base.curr_size_mblock_beats += block2->base.curr_size_mblock_beats;
        block1->next_free_block_ptr = block2->next_free_block_ptr;

        auto _next_mblock = ((mblock*)block1) + block1->base.curr_size_mblock_beats;
        if(_next_mblock!=region.end) _next_mblock->prev_size_mblock_beats = block1->base.curr_size_mblock_beats;

        auto next_block = block1->next_free_block_ptr;
        if(next_block) next_block->prev_free_block_ptr = block1;
    };

    // Unite with the previous
    {
        auto _pmblock1 = _this_mblock - _this_mblock->prev_size_mblock_beats;
        if(_this_mblock != region.root and !_pmblock1->is_occupied){
           unite((free_mblock*)_pmblock1, this_block);
           _this_mblock = _pmblock1;
           this_block = (free_mblock*)_this_mblock;
        }
    }

    // Unite with the next
    {
        auto _pnblock2 = _this_mblock + _this_mblock->curr_size_mblock_beats;
        if(_pnblock2 != region.end and !_pnblock2->is_occupied){
           unite(this_block, (free_mblock*)_pnblock2);
        }
    }
}

