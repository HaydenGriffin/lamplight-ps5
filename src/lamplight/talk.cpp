// Lamplight for PS5 - Push-to-talk: hold R2, speak, and Home Assistant's Assist answers.
// Copyright (C) 2026 Hayden Griffin
// SPDX-License-Identifier: GPL-3.0-or-later
#include "lamplight/application.hpp"
#include "lamplight/assistant.hpp"
#include "lamplight/paint.hpp"
#include <algorithm>
#include <cmath>

namespace lamplight
{
using hui::Action;
using hui::gfx::Align;
using hui::gfx::Rect;
namespace
{
// A held button longer than this is not a question; the client stops taking audio too.
constexpr float kMaxHoldS = 15;
// After letting go: how long the answer may take (speech to text, then the agent).
constexpr std::int64_t kAnswerMs = 30000;
constexpr std::int64_t kFollowUpMs = 5 * 60000;
// Quieter than this, the microphone hears nobody (muted, or too far away). Speech-to-text given
// only that makes up a word ("You"), so no run starts until something louder arrives.
constexpr float kSilence = 0.03f;
} // namespace

void Application::update_talk(const hui::InputFrame &input, float dt)
{
    if (talking_)
    {
        talk_held_s_ += dt;
        const float level = microphone_level ? microphone_level() : 0.0f;
        talk_peak_ = std::max(talk_peak_, level);
        // Home Assistant is asked to listen only once someone is speaking (the client keeps the
        // half second before), so silence never reaches the assistant as a made-up command.
        if (!talk_run_ && level >= kSilence)
        {
            talk_run_ = listen ? listen(config_.pipeline, talk_conversation_) : 0;
            if (!talk_run_)
            {
                if (close_microphone)
                    close_microphone();
                talking_ = false;
                toast("Not connected to Home Assistant", true, 5000);
                return;
            }
        }
        if (!input.is_held(Action::jump_next) || input.focus_lost || !input.connected ||
            talk_held_s_ > kMaxHoldS)
            finish_talk();
    }
    else if (input.is_pressed(Action::jump_next) && input.connected && !input.focus_lost &&
             !alert_ && page_ != Page::setup && page_ != Page::pick && page_ != Page::quit)
        start_talk();
    if (!talk_run_ || talking_)
        return;
    const auto &assist = snapshot_.assist;
    if (assist.run == talk_run_ &&
        (assist.stage == Assist::Stage::done || assist.stage == Assist::Stage::failed))
    {
        answer_talk(assist);
        talk_run_ = 0;
    }
    else if (talk_wait_until_ && now() > talk_wait_until_)
    {
        toast("No answer from Home Assistant yet", true, 5000);
        talk_run_ = 0;
    }
}
void Application::start_talk()
{
    if (read_only())
    {
        toast("Visitors mode is on, so nothing runs from here", true, 4000);
        return;
    }
    if (!ready())
    {
        toast("Not connected to Home Assistant", true, 5000);
        return;
    }
    if (!open_microphone || !open_microphone())
    {
        toast("No microphone. Check the controller's mute button, or plug in a headset.", true,
              6000);
        return;
    }
    const bool continuing = !conversation_.empty() && now() - conversation_at_ < kFollowUpMs;
    talk_conversation_ = continuing ? conversation_ : "";
    talk_run_ = 0;
    talking_ = true;
    talk_held_s_ = talk_peak_ = 0;
    talk_wait_until_ = 0;
    toast_ = {};
}
void Application::finish_talk()
{
    if (close_microphone)
        close_microphone();
    talking_ = false;
    talk_wait_until_ = now() + kAnswerMs;
    // Let go before anyone spoke: nothing was sent.
    if (!talk_run_)
        toast("The microphone heard nothing. Check the controller's mute button.", true, 6000);
}
void Application::answer_talk(const Assist &assist)
{
    if (assist.stage == Assist::Stage::failed)
    {
        toast(assist.error.empty() ? "Home Assistant couldn't answer" : assist.error, true, 6000);
        return;
    }
    auto reply = assist.reply;
    if (assist.heard.empty() || is_misfire(reply, config_.wake_phrase))
    {
        toast("Didn't catch that. Hold R2 and speak, then let go.", false, 5000);
        return;
    }
    if (reply.empty())
        reply = "Done";
    if (!assist.conversation.empty())
    {
        conversation_ = assist.conversation;
        conversation_at_ = now();
    }
    answer_ = reply;
    answer_title_.clear();
    answer_image_.clear();
    answer_at_ = now();
    info_scroll_ = 0;
    toast("\xE2\x80\x9C" + assist.heard + "\xE2\x80\x9D  " + plain_reply(reply), false, 8000,
          std::nullopt, "", true);
}
// While R2 is held: listening, with a meter; then what was heard while the answer comes.
void Application::draw_talk(Paint &p) const
{
    const auto &assist = snapshot_.assist;
    const bool waiting = talk_run_ && !talking_ && assist.run == talk_run_ &&
                         assist.stage != Assist::Stage::done &&
                         assist.stage != Assist::Stage::failed;
    if (!talking_ && !waiting)
        return;
    // On the clock, between the time and the chips; elsewhere, above the hints.
    const Rect r{530, page_ == Page::home ? 654.0f : 822.0f, 860, 104};
    p.list.shadow(r.inset(-2), size::chip_radius, 14, Color::rgb(0x000000, 0.5f));
    p.list.rounded_rect(r, size::chip_radius, Color::rgb(0x0c0d10));
    p.wash(r, size::chip_radius, tone::warm_glow.with_alpha(talking_ ? 0.45f : 0.2f));
    p.icon("microphone", r.x + 70, r.cy(), 64, talking_ ? tone::warm : tone::text);
    const float tx = r.x + 132;
    if (talking_)
    {
        p.text(talk_run_ && assist.run == talk_run_ && assist.stage == Assist::Stage::starting
                   ? "Starting\xE2\x80\xA6"
                   : "Listening\xE2\x80\xA6",
               tx, r.y + 46, size::title, tone::text, true);
        p.text("Let go of R2 when you've finished", tx, r.y + 84, size::caption, tone::muted);
        // The meter: twelve bars that rise with the voice.
        const float level = microphone_level ? std::clamp(microphone_level() * 3, 0.0f, 1.0f) : 0;
        for (int i = 0; i < 12; ++i)
        {
            const float lit = level * 12 > static_cast<float>(i) ? 1.0f : 0.25f;
            const float h = 14 + static_cast<float>(i % 4) * 8;
            p.list.rounded_rect(
                {r.x + r.w - 230 + static_cast<float>(i) * 17, r.cy() - h / 2, 9, h}, 4,
                tone::warm.with_alpha(lit));
        }
        return;
    }
    const auto heard = assist.heard.empty() ? std::string("Working out what you said\xE2\x80\xA6")
                                            : "\xE2\x80\x9C" + assist.heard + "\xE2\x80\x9D";
    p.text(heard, tx, r.y + 46, size::title, tone::text, true, Align::left, r.w - 170);
    p.text("Thinking\xE2\x80\xA6", tx, r.y + 84, size::caption, tone::muted);
}
} // namespace lamplight
