#include "pc2ps3/coop_input_router.hpp"

#include <cassert>

int main() {
 pc2ps3::CoopInputRouter router;
 pc2ps3::InputFrame first = {};
 pc2ps3::InputFrame second = {};
 first.left = true;
 first.attack_pressed = true;
 first.move_x = -1.0f;
 second.right = true;
 second.jump_pressed = true;
 second.move_x = 1.0f;

 assert(!router.ready(0) && !router.ready(1));
 assert(router.frame(0) == 0 && router.frame(2) == 0);
 assert(router.submit(0, first));
 assert(router.submit(1, second));
 const pc2ps3::InputFrame* first_frame = router.frame(0);
 const pc2ps3::InputFrame* second_frame = router.frame(1);
 assert(first_frame && first_frame->left && first_frame->attack_pressed);
 assert(first_frame->move_x == -1.0f && !first_frame->right);
 assert(second_frame && second_frame->right && second_frame->jump_pressed);
 assert(second_frame->move_x == 1.0f && !second_frame->left);

 pc2ps3::InputFrame replacement = {};
 replacement.cancel_pressed = true;
 assert(!router.submit(0, replacement));
 assert(router.frame(0)->attack_pressed && !router.frame(0)->cancel_pressed);
 assert(!router.submit(2, replacement));

 router.begin_tick();
 assert(!router.ready(0) && !router.ready(1));
 assert(router.frame(0) == 0 && router.frame(1) == 0);
 assert(router.submit(1, replacement));
 assert(router.frame(1)->cancel_pressed);
 assert(!router.ready(0));
 return 0;
}
