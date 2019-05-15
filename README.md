**Ecs** is a single header Entity Component System library for C game development.

To use the library you must `#define ECS_IMPLEMENTATION` in at least **ONE** .c file.

Example:

```c
#include xxx
#include xxx
#include xxx

#define ECS_IMPLEMENTATION
#include "ecs.h"
```

You can `#define` the following (must be defined before `define #ECS_IMPLEMENTATION`):

- `#define ECS_STATIC` for a static implementation of the library.

- `#define ECS_ENABLE_LOGGING` for warnings when invalid operations occur.

- `#define ECS_MALLOC` to provide your own malloc.

- `#define ECS_FREE` to provide your own free.


Example usage (some of it is psuedo code):

```c
#define ECS_IMPLEMENTATION
#include "ecs.h"

//Define our components 
typedef struct
{
    float x, y;
} CTransform;

typedef struct
{
    float dx, dy;
} CVelocity;

typedef struct
{
    u32 gl_id;
    float rotation;
} CSprite;

typedef struct
{
    EcsEnt target;
} CMissle;

typedef enum
{
    COMPONENT_TRANSFORM,
    COMPONENT_VELOCITY,
    COMPONENT_SPRITE,
    COMPONENT_MISSLE,

    COMPONENT_COUNT
} ComponentType;

//ECS_MASK takes the number of components, and then the components
//if you only need to check if the entity has one component,
//you can optionally use ecs_ent_has_component

#define MOVEMENT_SYSTEM_MASK \
ECS_MASK(2, COMPONENT_TRANSFORM, COMPONENT_VELOCITY)
void 
movement_system(Ecs *ecs)
{
    for (u32 i = 0; i < ecs_for_count(ecs); i++)
    {
        EcsEnt e = ecs_get_ent(ecs, i);
        if (ecs_ent_has_mask(ecs, e, MOVEMENT_SYSTEM_MASK))
        {
            CTransform *xform   = ecs_ent_get_component(ecs, e, COMPONENT_TRANSFORM);
            CVelocity *velocity = ecs_ent_get_component(ecs, e, COMPONENT_VELOCITY);

            xform->x += velocity->dx;
            xform->y += velocity->dy;
        }
    }
}

#define SPRITE_RENDER_SYSTEM_MASK \
ECS_MASK(2, COMPONENT_TRANSFORM, COMPONENT_SPRITE)
void 
sprite_render_system(Ecs *ecs)
{
    for (u32 i = 0; i < ecs_for_count(ecs); i++)
    {
        EcsEnt e = ecs_get_ent(ecs, i);
        if (ecs_ent_has_mask(ecs, e, SPRITE_RENDER_SYSTEM_MASK))
        {
            CTransform *xform = ecs_ent_get_component(ecs, e, COMPONENT_TRANSFORM);
            CSprite *sprite   = ecs_ent_get_component(ecs, e, COMPONENT_SPRITE);
            
            render_sprite(sprite->gl_id, sprite->rot);
        }
    }
}

#define MISSLE_SYSTEM_MASK \
ECS_MASK(2, COMPONENT_TRANSFORM, COMPONENT_MISSLE)
void
missle_system(Ecs *ecs)
{ 
    for (u32 i = 0; i < ecs_for_count(ecs); i++)
    {
        EcsEnt e = ecs_get_ent(ecs, i);
        if (ecs_ent_has_mask(ecs, e, MISSLE_SYSTEM_MASK))
        {
            CTransform *xform = ecs_ent_get_component(ecs, e, COMPONENT_TRANSFORM);
            CMissle *missle   = ecs_ent_get_component(ecs, e, COMPONENT_SPRITE);
            
            //when storing a reference to an EcsEnt we must check if the entity is valid before
            //operating on it
            if (ecs_ent_is_valid(missle->target))
            {
                xform->x += towards_target.x;
                xform->y += towards_target.y;
            }
            //or maybe just remove the component if its not valid, all depends on the situation 
        }
    }
}

void 
register_components(Ecs *ecs)
{
    //Ecs, component index, component pool size, size of component, and component free func
    ecs_register_component(ecs, COMPONENT_TRANSFORM, 1000, sizeof(CTransform), NULL);
    ecs_register_component(ecs, COMPONENT_VELOCITY, 200, sizeof(CVelocity), NULL);
    ecs_register_component(ecs, COMPONENT_SPRITE, 1000, sizeof(CSprite), NULL);
    ecs_register_component(ecs, COMPONENT_MISSLE, 10, sizeof(CMissle), NULL);
}

void 
register_systems(Ecs *ecs)
{
    //ecs_run_systems will run the systems in the order they are registered
    //ecs_run_system is also available if you wish to handle each system seperately
    //
    //Ecs, function pointer to system (must take a parameter of Ecs), system type
    ecs_register_system(ecs, movement_system, ECS_SYSTEM_UPDATE);
    ecs_register_system(ecs, missle_system, ECS_SYSTEM_UPDATE);
    ecs_register_system(ecs, sprite_render_system, ECS_SYSTEM_RENDER);
}

int
main(int argc, char **argv)
{
    //Max entities, component count, system_count
    Ecs *ecs = ecs_make(1000, COMPONENT_COUNT, 3);
    register_components(ecs);
    register_systems(ecs);

    EcsEnt e = ecs_ent_make(ecs);
    CTransform xform = {0, 0};
    CVelocity velocity = {5, 0};
    ecs_ent_add_component(ecs, e, COMPONENT_TRANSFORM, &xform);
    ecs_ent_add_component(ecs, e, COMPONENT_VELOCITY, &velocity);
    
    //.... do whatever
    
    ecs_ent_destroy(ecs, e);
    
    //eng loop code
    while(1)
    {
        if (update)
        {
            ecs_run_systems(ecs, ECS_SYSTEM_UPDATE);
        }
        if (render)
        {
            ecs_run_systems(ecs, ECS_SYSTEM_RENDER);
        }
    }

    ecs_destroy(ecs);
}
```
