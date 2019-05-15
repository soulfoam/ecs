#ifndef E_C_S__
#define E_C_S__

#include <stdint.h>
#include <stdbool.h>

#ifdef ECS_ENABLE_LOGGING
#define ecs_warn(s_, ...) printf(s_ "\n", ##__VA_ARGS__)
#else
#define ecs_warn(s_, ...)
#endif

#ifdef ECS_STATIC
#define ECSDEF static
#else
#define ECSDEF extern
#endif

#ifndef ECS_MALLOC
#define ECS_MALLOC malloc
#endif

#ifndef ECS_FREE
#define ECS_FREE free
#endif

typedef uint64_t EcsEnt;
typedef uint32_t EcsComponentType;

#define ECS_MASK(ctypes_count, ...) \
    ctypes_count, (EcsComponentType[]){__VA_ARGS__}

typedef enum
{
    ECS_SYSTEM_UPDATE,
    ECS_SYSTEM_RENDER
} EcsSystemType;

typedef struct Ecs Ecs;

typedef void (*ecs_system_func)(struct Ecs *ecs);
typedef void (*ecs_component_destroy)(void *data);

ECSDEF Ecs*      ecs_make(uint32_t max_entities, uint32_t component_count, uint32_t system_count);
ECSDEF void      ecs_destroy(Ecs *ecs);
ECSDEF void      ecs_register_component(Ecs *ecs, 
                                        EcsComponentType component_type, 
                                        uint32_t count, uint32_t size, 
                                        ecs_component_destroy destroy_func);
ECSDEF void      ecs_register_system(Ecs *ecs, ecs_system_func func, EcsSystemType type);
ECSDEF void      ecs_run_systems(Ecs *ecs, EcsSystemType type);
ECSDEF void      ecs_run_system(Ecs *ecs, uint32_t system_index);
ECSDEF uint32_t  ecs_for_count(Ecs *ecs);
ECSDEF EcsEnt    ecs_get_ent(Ecs *ecs, uint32_t index);
ECSDEF EcsEnt    ecs_ent_make(Ecs *ecs);
ECSDEF void      ecs_ent_destroy(Ecs *ecs, EcsEnt e);
ECSDEF void      ecs_ent_add_component(Ecs *ecs, 
                                       EcsEnt e, 
                                       EcsComponentType type, void *component_data);
ECSDEF void      ecs_ent_remove_component(Ecs *ecs, EcsEnt e, EcsComponentType type);
ECSDEF void*     ecs_ent_get_component(Ecs *ecs, EcsEnt e, EcsComponentType type);
ECSDEF bool      ecs_ent_has_component(Ecs *ecs, EcsEnt e, EcsComponentType component_type);
ECSDEF bool      ecs_ent_has_mask(Ecs *ecs, EcsEnt e,
                       uint32_t component_type_count, EcsComponentType component_types[]);
ECSDEF bool      ecs_ent_is_valid(Ecs *ecs, EcsEnt e);
ECSDEF uint32_t  ecs_ent_get_version(Ecs *ecs, EcsEnt e);
ECSDEF void      ecs_ent_print(Ecs *ecs, EcsEnt e);

#endif



#ifdef ECS_IMPLEMENTATION

#include <stdio.h> 
#include <stdlib.h>
#include <string.h>

#define ECS_ENT_ID(index, ver) (((uint64_t)ver << 32) | index)
#define ECS_ENT_INDEX(id) ((uint32_t)id)
#define ECS_ENT_VER(id) ((uint32_t)(id >> 32))

typedef struct
{
    uint32_t *data;
    size_t   capacity;
    size_t   top;
    bool     empty;
} EcsStack;

typedef struct
{
	void *data;
	uint32_t count;
	uint32_t size;

    ecs_component_destroy destroy_func;

    EcsStack *indexes;
	
} EcsComponentPool;

typedef struct
{
    ecs_system_func func;
    EcsSystemType type;
} EcsSystem;

struct
Ecs
{	
    uint32_t max_entities; 
    uint32_t component_count;
    uint32_t system_count;

    EcsStack *indexes;

    //max_index is a "mini" optimization and should be improved
    //upon sometime soon. 
    uint32_t max_index;
    uint32_t *versions;

    //components are the indexes into components.. 
    //max entities * component_count 
    //index via (index * comp_count + comp_type)
    //component_masks works the same, it just checks if the mask is enabled
    uint32_t *components;
    bool *component_masks;

	EcsComponentPool *pool;

    EcsSystem *systems;
    uint32_t  systems_top;

};

ECSDEF EcsStack*
ecs_stack_make(size_t capacity)
{
    EcsStack *s   = ECS_MALLOC(sizeof(*s));
    s->data       = ECS_MALLOC(sizeof(*s->data) * capacity);
    s->capacity   = capacity;
    s->top        = 0;
    s->empty      = true;

    return s;
}

ECSDEF void
ecs_stack_destroy(EcsStack *s)
{
    ECS_FREE(s->data);
    ECS_FREE(s);
}

ECSDEF bool
ecs_stack_empty(EcsStack *s)
{
    return s->empty;
}

ECSDEF bool
ecs_stack_full(EcsStack *s)
{
    return s->top == s->capacity;
}

ECSDEF size_t
ecs_stack_capacity(EcsStack *s)
{
    return s->capacity;
}

ECSDEF size_t
ecs_stack_top(EcsStack *s)
{
    return s->top;
}

ECSDEF uint32_t
ecs_stack_peek(EcsStack *s)
{
    if (s->empty)
    {
        ecs_warn("Failed to peek, stack is full", val);
        return 0;
    }
    return s->data[s->top-1];
}

ECSDEF void
ecs_stack_push(EcsStack *s, uint32_t val)
{
    if (ecs_stack_full(s)) 
    {
        ecs_warn("Failed to push %u, stack is full", val);
        return;
    } 

    s->empty = false;
    s->data[s->top++] = val;
}

ECSDEF uint32_t
ecs_stack_pop(EcsStack *s)
{
    if (s->empty)
    {
        ecs_warn("Failed to pop, stack is empty", val);
        return 0;
    }

    if (s->top == 1) s->empty = true;
    return s->data[--s->top];
}

ECSDEF EcsComponentPool
ecs_component_pool_make(uint32_t count, uint32_t size, ecs_component_destroy destroy_func)
{
    EcsComponentPool pool;
    pool.data         = ECS_MALLOC(count * size);
    pool.count        = count;
    pool.size         = size;
    pool.destroy_func = destroy_func;
    pool.indexes      = ecs_stack_make(count);

    for (uint32_t i = count; i --> 0; )
    {
        ecs_stack_push(pool.indexes, i);
    }

    return pool;
}

ECSDEF void
ecs_component_pool_destroy(EcsComponentPool *pool)
{
    ECS_FREE(pool->data);
    ecs_stack_destroy(pool->indexes);
}

ECSDEF void
ecs_component_pool_push(EcsComponentPool *pool, uint32_t index)
{
    uint8_t *ptr = (uint8_t*)(pool->data + (index * pool->size));
    if (pool->destroy_func) pool->destroy_func(ptr);
    ecs_stack_push(pool->indexes, index);
}

ECSDEF uint32_t
ecs_component_pool_pop(EcsComponentPool *pool, void *data)
{
    uint32_t index = ecs_stack_pop(pool->indexes);
    uint8_t *ptr   = (uint8_t*)(pool->data + (index * pool->size));
    memcpy(ptr, data, pool->size);
    return index;
}

ECSDEF Ecs*
ecs_make(uint32_t max_entities, uint32_t component_count, uint32_t system_count)
{
	Ecs *ecs             = ECS_MALLOC(sizeof(*ecs));
    ecs->max_entities    = max_entities;
    ecs->component_count = component_count;
    ecs->system_count    = system_count;
    ecs->indexes         = ecs_stack_make(max_entities);
    ecs->max_index       = 0;
    ecs->versions        = ECS_MALLOC(max_entities * sizeof(uint32_t));
    ecs->components      = ECS_MALLOC(max_entities * component_count * sizeof(uint32_t));
    ecs->component_masks = ECS_MALLOC(max_entities * component_count * sizeof(bool));
    ecs->pool            = ECS_MALLOC(component_count * sizeof(*ecs->pool));
    ecs->systems         = ECS_MALLOC(system_count * sizeof(*ecs->systems));
    ecs->systems_top     = 0;

	for (uint32_t i = max_entities; i --> 0;)
	{	
        ecs_stack_push(ecs->indexes, i);

        ecs->versions[i] = 0;
		for (uint32_t j = 0; j < component_count; j++)
        {
            ecs->components[i * component_count + j]      = 0;
            ecs->component_masks[i * component_count + j] = 0;
        }
	}

    for (uint32_t i = 0; i < system_count; i++)
    {
        ecs->systems[i].func = NULL;
    }

	for (uint32_t i = 0; i < ecs->component_count; i++)
    {
        ecs->pool[i].data = NULL;
    }
    
	return ecs;
}

ECSDEF void
ecs_destroy(Ecs *ecs)
{
	for (uint32_t i = 0; i < ecs->component_count; i++)
	{
		ecs_component_pool_destroy(&ecs->pool[i]);
	}
    
    ecs_stack_destroy(ecs->indexes);

    ECS_FREE(ecs->versions);
    ECS_FREE(ecs->components);
    ECS_FREE(ecs->component_masks);
    ECS_FREE(ecs->pool);
    ECS_FREE(ecs->systems);

	ECS_FREE(ecs);
}

ECSDEF void          
ecs_register_component(Ecs *ecs, 
        EcsComponentType component_type, 
        uint32_t count, uint32_t size, 
        ecs_component_destroy destroy_func)
{
    if (ecs->pool[component_type].data != NULL)
    {
        ecs_warn("Registered Component type %lu more than once.\n", component_type);
        return;
    }

    ecs->pool[component_type] = ecs_component_pool_make(count, size, destroy_func);
}

ECSDEF void          
ecs_register_system(Ecs *ecs, ecs_system_func func, EcsSystemType type)
{
    EcsSystem *sys = &ecs->systems[ecs->systems_top++];
    sys->func = func;
    sys->type = type;
}

ECSDEF void
ecs_run_systems(Ecs *ecs, EcsSystemType type)
{
    for (uint32_t i = 0; i < ecs->systems_top; i++)
    {
        EcsSystem *sys = &ecs->systems[i];
        if (sys->type == type) sys->func(ecs);
    }
}

ECSDEF void
ecs_run_system(Ecs *ecs, uint32_t system_index)
{
    ecs->systems[system_index].func(ecs);
}

ECSDEF uint32_t        
ecs_for_count(Ecs *ecs)
{
    return ecs->max_index+1; 
}

ECSDEF EcsEnt
ecs_get_ent(Ecs *ecs, uint32_t index)
{
    return ECS_ENT_ID(index, ecs->versions[index]);
}

ECSDEF EcsEnt
ecs_ent_make(Ecs *ecs)
{
    uint32_t index = ecs_stack_pop(ecs->indexes);
    uint32_t ver   = ecs->versions[index]; 

    if (index > ecs->max_index) ecs->max_index = index;

    return ECS_ENT_ID(index, ver);
}

ECSDEF void
ecs_ent_destroy(Ecs *ecs, EcsEnt e)
{	
    uint32_t index = ECS_ENT_INDEX(e);

    ecs->versions[index]++;
	for (uint32_t i = 0; i < ecs->component_count; i++)
	{
		ecs_ent_remove_component(ecs, e, i);
	}

    ecs_stack_push(ecs->indexes, index);
}

ECSDEF void
ecs_ent_add_component(Ecs *ecs, EcsEnt e, EcsComponentType type, void *component_data)
{
    uint32_t index = ECS_ENT_INDEX(e);

	if (ecs_ent_has_component(ecs, e, type))
	{
		ecs_warn("Component %u already exists on EcsEnt %u", type, ECS_ENT_INDEX(e));
		return;
	}

	EcsComponentPool *pool = &ecs->pool[type];
    uint32_t c_index = ecs_component_pool_pop(pool, component_data);
	ecs->components[index * ecs->component_count + type] = c_index;
	ecs->component_masks[index * ecs->component_count + type] = true;
}

ECSDEF void
ecs_ent_remove_component(Ecs *ecs, EcsEnt e, EcsComponentType type)
{
    uint32_t index = ECS_ENT_INDEX(e);

	if (!ecs_ent_has_component(ecs, e, type))
	{
		ecs_warn("Component %u doesn't exist on EcsEnt %u", type, index);
		return;
	}

	EcsComponentPool *pool = &ecs->pool[type];
    ecs_component_pool_push(pool, ecs->components[index * ecs->component_count + type]);
	ecs->component_masks[index * ecs->component_count + type] = false;
}

ECSDEF void*
ecs_ent_get_component(Ecs *ecs, EcsEnt e, EcsComponentType type)
{	
    uint32_t index = ECS_ENT_INDEX(e);

	if (!ecs_ent_has_component(ecs, e, type))
	{
		ecs_warn("Trying to get non existing Component %lu on EcsEnt %u", type, index);
		return NULL;
	}

    uint32_t c_index = ecs->components[index * ecs->component_count + type];
    uint8_t *ptr = (uint8_t*)(ecs->pool[type].data + (c_index * ecs->pool[type].size));
	return ptr;
}

ECSDEF bool
ecs_ent_is_valid(Ecs *ecs, EcsEnt e)
{
    return ecs->versions[ECS_ENT_INDEX(e)] == ECS_ENT_VER(e);
}

ECSDEF bool          
ecs_ent_has_component(Ecs *ecs, EcsEnt e, EcsComponentType component_type)
{
    return ecs->component_masks[ECS_ENT_INDEX(e) * ecs->component_count + component_type];
}

ECSDEF bool          
ecs_ent_has_mask(Ecs *ecs, 
                       EcsEnt e, 
                       uint32_t component_type_count, EcsComponentType *component_types)
{
    for (uint32_t i = 0; i < component_type_count; i++)
    {
        if (!ecs_ent_has_component(ecs, e, component_types[i])) return false;
    }

    return true;
}

ECSDEF uint32_t           
ecs_ent_get_version(Ecs *ecs, EcsEnt e)
{
    return ecs->versions[ECS_ENT_INDEX(e)];
}

ECSDEF void
ecs_ent_print(Ecs *ecs, EcsEnt e)
{
    uint32_t index = ECS_ENT_INDEX(e);

	printf("---- EcsEnt ----\nIndex: %d\nVersion: %d\nMask: ", ECS_ENT_INDEX(e), ecs->versions[index]);

	for(uint32_t i = ecs->component_count; i --> 0;) 
    {
        printf("%u", ecs->component_masks[index * ecs->component_count + i]);
    }
    printf("\n");

	for (uint32_t i = 0; i < ecs->component_count; i++)
	{
		printf("Component Type: %d (Index: %d)\n", i, 
                ecs->components[index * ecs->component_count + i]);
	}

	printf("----------------\n");

}


#endif

