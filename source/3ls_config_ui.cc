#include "3ls_config.hh"

#include <ui/base.hh>
#include <ui/swkbd.hh>

namespace
{
	void clean_server(std::string& value)
	{
		if(value.find("https://") == 0) value.erase(0, 8);
		if(value.find("http://") == 0) value.erase(0, 7);
		for(std::string::size_type pos; (pos = value.find(':')) != std::string::npos;)
			value.erase(pos, 1);
		for(std::string::size_type pos; (pos = value.find('\n')) != std::string::npos;)
			value.erase(pos, 1);
	}
}

void ls_config::show_editor()
{
	ui::I18NEnabledRenderQueue queue;
	ui::Button *server, *port, *username, *token;
	Config edited = get();
	constexpr float width = ui::screen_width(ui::Screen::bottom) - 20.0f;
	constexpr float height = 20.0f;
	auto edit_text = [](ui::Button *button, std::string *value, const char *hint, bool password) -> bool {
		ui::RenderQueue::global()->render_and_then([button, value, hint, password]() -> void {
			SwkbdButton pressed;
			std::string result = ui::keyboard([&](ui::AppletSwkbd *keyboard) -> void {
				keyboard->hint(hint); keyboard->init_text(*value);
				if(password) keyboard->passmode(SWKBD_PASSWORD_HIDE_DELAY);
			}, &pressed);
			if(pressed == SWKBD_BUTTON_CONFIRM) {
				*value = result;
				button->set_label(password && !result.empty() ? "(configured)" : result);
			}
		});
		return true;
	};
	ui::builder<ui::Text>(ui::Screen::top, "3LS server and HSAPI authentication")
		.wrap().x(ui::layout::center_x).y(ui::layout::center_y).add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, "Server address").x(10).y(10).add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom, edited.server).x(10)
		.when_clicked([&server, &edited, edit_text](ui::Button *button) -> bool { return edit_text(button, &edited.server, "Server address", false); })
		.size(width, height).under(queue.back()).add_to(&server, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, "Port").x(10).under(queue.back()).add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom, edited.port ? std::to_string(edited.port) : "").x(10)
		.when_clicked([&port, &edited](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then([&port, &edited]() -> void {
				SwkbdButton pressed;
				uint64_t value = ui::numpad([](ui::AppletSwkbd *keyboard) -> void { keyboard->hint("Port"); }, &pressed, nullptr, 5);
				if(pressed == SWKBD_BUTTON_CONFIRM && value != 0) {
					edited.port = value & 0xffff; port->set_label(std::to_string(edited.port));
				}
			});
			return true;
		}).size(width, height).under(queue.back()).add_to(&port, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, "HSAPI username").x(10).under(queue.back()).add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom, edited.username).x(10)
		.when_clicked([&username, &edited, edit_text](ui::Button *button) -> bool { return edit_text(button, &edited.username, "HSAPI username", false); })
		.size(width, height).under(queue.back()).add_to(&username, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, "HSAPI token").x(10).under(queue.back()).add_to(queue);
	ui::builder<ui::Button>(ui::Screen::bottom, edited.token.empty() ? "" : "(configured)").x(10)
		.when_clicked([&token, &edited, edit_text](ui::Button *button) -> bool { return edit_text(button, &edited.token, "HSAPI token", true); })
		.size(width, height).under(queue.back()).add_to(&token, queue);
	ui::Button *clear = nullptr;
	ui::builder<ui::Button>(ui::Screen::bottom, str::clear)
		.when_clicked([server, port, username, token, &edited](ui::Button *) -> bool {
			edited.server.clear();
			edited.port = 0;
			edited.username.clear();
			edited.token.clear();
			server->set_label("");
			port->set_label("");
			username->set_label("");
			token->set_label("");
			return true;
		})
		.x(ui::layout::right).y(ui::layout::bottom).wrap().add_to(&clear, queue);
	clear->set_x(clear->get_x() - clear->width());
	clear->set_y(clear->get_y() - clear->height());
	queue.render_finite_button(KEY_B | KEY_A);
	clean_server(edited.server);
	set(edited);
	save();
}
