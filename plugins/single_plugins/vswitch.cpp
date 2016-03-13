#include <core.hpp>

class VSwitch : public Plugin {
    private:
        KeyBinding kbs[4];
        uint32_t switch_workspaceBindings[4];

        Hook hook;
        int stepNum;
        int vstep;
        int dx, dy;
        int nx, ny;
        std::queue<std::tuple<int, int> >dirs; // series of moves we have to do
    public:

    void initOwnership() {
        owner->name = "vswitch";
        owner->compatAll = false;
        owner->compat.insert("move");
    }

    void updateConfiguration() {
        vstep = getSteps(options["duration"]->data.ival);
    }

    void beginSwitch() {
        auto tup = dirs.front();
        dirs.pop();

        GetTuple(ddx, ddy, tup);
        GetTuple(vx, vy, core->get_current_viewport());
        GetTuple(vw, vh, core->get_viewport_grid_size());
        GetTuple(sw, sh, core->getScreenSize());

        nx = (vx - ddx + vw) % vw;
        ny = (vy - ddy + vh) % vh;
        core->switch_workspace(std::make_tuple(nx, ny));

        dx = (vx - nx) * sw;
        dy = (vy - ny) * sh;

        uint32_t new_mask = core->get_mask_for_viewport(nx, ny);
        uint32_t old_mask = core->get_mask_for_viewport(vx, vy);

        core->for_each_window([=] (View v) {
            if(v->default_mask & new_mask)
                v->transform.translation =
                    glm::translate(glm::mat4(), glm::vec3(2.f * (nx - vx), 2.f * (vy - ny), 0));
        });

        core->set_redraw_everything(true);
        core->set_renderer(new_mask | old_mask);
        core->activate_owner(owner);
        stepNum = 0;
    }

#define MAXDIRS 6
    void insertNextDirection(int ddx, int ddy) {
        if(!hook.getState())
            hook.enable(),
            dirs.push(std::make_tuple(ddx, ddy)),
            beginSwitch();
        else if(dirs.size() < MAXDIRS)
            dirs.push(std::make_tuple(ddx, ddy));
    }

    void handleKey(Context ctx) {

        if (!core->activate_owner(owner))
            return;

        owner->grab();

        auto xev = ctx.xev.xkey;

        if(xev.key == switch_workspaceBindings[0])
            insertNextDirection(1,  0);
        if(xev.key == switch_workspaceBindings[1])
            insertNextDirection(-1, 0);
        if(xev.key == switch_workspaceBindings[2])
            insertNextDirection(0, -1);
        if(xev.key == switch_workspaceBindings[3])
            insertNextDirection(0,  1);
    }

    void Step() {
        GetTuple(w, h, core->getScreenSize());

        if (stepNum == vstep) {
            Transform::gtrs = glm::mat4();
                     core->set_redraw_everything(false);
            core->reset_renderer();

            auto views = core->get_windows_on_viewport(core->get_current_viewport());
            for(auto v : views) {
                v->transform.translation = glm::mat4();
            }

            if (dirs.size() == 0) {
                hook.disable();
                core->deactivate_owner(owner);
            } else {
                beginSwitch();
            }

            return;
        }

        float progress = float(stepNum++) / float(vstep);

        float offx =  2.f * progress * float(dx) / float(w);
        float offy = -2.f * progress * float(dy) / float(h);

        Transform::gtrs = glm::translate(glm::mat4(), glm::vec3(offx, offy, 0.0));
    }

    SignalListener viewport_change_request;

    void init() {
        using namespace std::placeholders;

        options.insert(newIntOption("duration", 500));

        switch_workspaceBindings[0] = XKB_KEY_h;
        switch_workspaceBindings[1] = XKB_KEY_l;
        switch_workspaceBindings[2] = XKB_KEY_j;
        switch_workspaceBindings[3] = XKB_KEY_k;

        for(int i = 0; i < 4; i++) {
            kbs[i].type = BindingTypePress;
            kbs[i].mod = WLC_BIT_MOD_CTRL | WLC_BIT_MOD_ALT;
            kbs[i].key = switch_workspaceBindings[i];
            kbs[i].action = std::bind(std::mem_fn(&VSwitch::handleKey), this, _1);
            core->add_key(&kbs[i], true);
        }

        hook.action = std::bind(std::mem_fn(&VSwitch::Step), this);
        core->add_hook(&hook);

        viewport_change_request.action =
            std::bind(std::mem_fn(&VSwitch::on_viewport_change_request), this, _1);

        core->connect_signal("viewport-change-request", &viewport_change_request);
    }

    void on_viewport_change_request(SignalListenerData data) {
        GetTuple(vx, vy, core->get_current_viewport());
        int nx = *(int*)data[0];
        int ny = *(int*)data[1];

        int dx = nx - vx;
        int dy = ny - vy;

        if (!dx && !dy)
            return;

        /* Do not deny request if we cannot activate owner,
         * it might have come from another plugin which is incompatible with us (for ex. expo) */
        if (!core->activate_owner(owner)) {
            core->switch_workspace(std::make_tuple(nx, ny));
            return;
        }

        std::cout << "viewport change request" << std::endl;
        std::cout << dx << " *** " << dy << std::endl;

        int dirx = dx > 0 ? 1 : -1;
        int keyx = dx > 0 ? switch_workspaceBindings[1] : switch_workspaceBindings[0];
        int diry = dy > 0 ? 1 : -1;
        int keyy = dy > 0 ? switch_workspaceBindings[3] : switch_workspaceBindings[2];

        Context ctx(0, 0, 0, 0);

        while (vx != nx) {
            ctx.xev.xkey.key = keyx;
            handleKey(ctx);
            vx += dirx;
        }

        while (vy != ny) {
            ctx.xev.xkey.key = keyy;
            handleKey(ctx);
            vy += diry;
        }

    }
};
extern "C" {
    Plugin *newInstance() {
        return new VSwitch();
    }
}