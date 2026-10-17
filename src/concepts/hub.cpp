// PS5 Hub - Interface native française et navigation DualSense.
// Copyright (C) 2026 PS5 Hub contributors
// SPDX-License-Identifier: GPL-3.0-or-later
#include "concepts/concepts.hpp"
#include "hub/service.hpp"
#include "ui/components/list.hpp"
#include "ui/components/grid.hpp"
#include "ui/components/dialog.hpp"
#include "ui/components/keyboard.hpp"
#include "ui/glyphs.hpp"
#include "core/save_file.hpp"
#include "third_party/qrcodegen/qrcodegen.h"
#include <set>
#include <cmath>
#include <ctime>
#include <cstdlib>

namespace hui::concepts
{
namespace
{
using gfx::Color;
constexpr const char *sections[] = {"Accueil",      "Jeux",     "Homebrew",
                                    "Applications", "Fichiers", "Paramètres"};
constexpr const char *subtitles[] = {"Découvrez les créations de la communauté PS5.",
                                     "Jeux, portages homebrew et votre bibliothèque personnelle.",
                                     "Découvrez des projets libres et ajoutez vos archives.",
                                     "Vos archives d’applications et vos outils.",
                                     "Les fichiers réellement présents dans votre stockage.",
                                     "Une interface à votre rythme."};
constexpr const char *techniques[] = {"Rendu OpenGL natif",
                                      "Téléchargements HTTPS sur un travailleur",
                                      "Démarrage signalé sur 13.60, validation partielle"};
#ifdef HUB_PS5
constexpr const char *data_root = "/download0/hub";
constexpr const char *assets_root = "/app0/assets";
#else
constexpr const char *data_root = "build/preview-data";
constexpr const char *assets_root = "assets";
#endif
const char *asset_path()
{
#ifndef HUB_PS5
    if (const auto *path = std::getenv("HUB_ASSETS"))
        return path;
#endif
    return assets_root;
}
ui::ListItem row(std::string title, std::string subtitle = {}, std::string badge = {})
{
    ui::ListItem r;
    r.title = std::move(title);
    r.subtitle = std::move(subtitle);
    r.badge = std::move(badge);
    r.chevron = true;
    return r;
}
class Hub final : public app::Concept
{
  public:
    explicit Hub(app::Context &c) : context_(c), service_(data_root, asset_path())
    {
        theme_ = ui::themes()[0];
        theme_.surface = Color::rgb(0x151e30);
        theme_.surface_high = Color::rgb(0x22314a);
        theme_.primary = theme_.accent = Color::rgb(0x80edd4);
        theme_.on_primary = Color::rgb(0x07151a);
        theme_.text = Color::rgb(0xf1f5fc);
        theme_.text_muted = Color::rgb(0xa6b7ce);
        theme_.focus = Color::rgb(0x80edd4);
        theme_.style = ui::SurfaceStyle::soft;
        grid_.style.theme = theme_;
        grid_.style.card.plate = true;
        grid_.style.card.glow = true;
        grid_.set_bounds({504, 432, 1320, 420});
        nav_.style.theme = items_.style.theme = dialog_.style.theme = keys_.style.theme = theme_;
        nav_.set_bounds({96, 270, 340, 590});
        nav_.style.row_height = 82;
        std::vector<ui::ListItem> navigation;
        for (const auto *s : sections)
            navigation.push_back(row(s));
        nav_.set_items(std::move(navigation));
        items_.set_bounds({504, 432, 1320, 420});
        items_.style.row_height = 104;
        items_.style.cards = true;
        items_.style.gap = 14;
        items_.set_active(false);
        keys_.set_bounds({480, 470, 1344, 390});
        keys_.style.done_label = "Valider";
        keys_.style.space_label = "Espace";
        keys_.style.bindings = ui::KeyboardBindings::standard();
        keys_.style.max_length = 120;
        keys_.on_text = [this](std::string_view text)
        {
            edit_error_.clear();
            if (edit_.size() + text.size() <= static_cast<std::size_t>(keys_.style.max_length))
                edit_ += text;
        };
        keys_.on_backspace = [this]
        {
            edit_error_.clear();
            if (!edit_.empty())
            {
                auto pos = edit_.size() - 1;
                while (pos > 0 && (static_cast<unsigned char>(edit_[pos]) & 0xc0) == 0x80)
                    --pos;
                edit_.erase(pos);
            }
        };
        std::string saved;
        if (hui::save::read_file(std::string(data_root) + "/favorites.txt", &saved, 350000))
        {
            std::size_t start = 0;
            while (start < saved.size() && favorites_.size() < 5000)
            {
                auto end = saved.find('\n', start);
                auto id = saved.substr(start, end == saved.npos ? saved.size() - start : end - start);
                if (!id.empty() && id.size() <= 64)
                    favorites_.insert(id);
                if (end == saved.npos)
                    break;
                start = end + 1;
            }
        }
#ifdef HUB_PS5
        sync_pending_ = true;
#endif
        refresh();
        service_.load();
    }
    ~Hub() override
    {
        release_images();
    }
    const app::ConceptInfo &info() const override
    {
        static const app::ConceptInfo info{"hub",
                                           "PS5 Hub",
                                           "Votre bibliothèque homebrew",
                                           "src/concepts/hub.cpp",
                                           audio::SoundSet::glass,
                                           Color::rgb(0x80edd4),
                                           techniques};
        return info;
    }
    void update(const InputFrame &input, float dt, app::Feedback &feedback) override
    {
        if (!context_.settings.reduced_motion)
            clock_ += dt;
        nav_.style.reduced_motion = items_.style.reduced_motion = dialog_.style.reduced_motion =
            keys_.style.reduced_motion = grid_.style.reduced_motion =
                context_.settings.reduced_motion;
        if (service_.poll(snapshot_))
        {
            upload_images();
            refresh();
        }
        if (sync_pending_ && !snapshot_.busy)
        {
            sync_pending_ = false;
            service_.sync_catalog();
        }
        if (dialog_.is_open())
        {
            if (dialog_.handle(input, feedback) == ui::Event::activated && dialog_.choice() == 1)
            {
                if (pending_download_)
                    service_.download(selected_);
            }
        }
        else if (detail_open_)
        {
            if (input.is_pressed(Action::back))
                detail_open_ = false;
            else if (input.is_pressed(Action::menu))
                toggle_favorite(selected_, feedback);
            else if (input.is_pressed(Action::confirm))
            {
                if (hub::is_reference(selected_))
                    notice("Ouvrir sur votre téléphone", "Scannez le QR de la fiche pour consulter le projet et ses instructions.", feedback);
                else if (snapshot_.busy)
                    notice("Opération en cours", "Attendez ou annulez avec Carré après avoir fermé la fiche.", feedback);
                else
                {
                    pending_download_ = true;
                    dialog_.open({ui::StatusKind::none, "Télécharger cette archive ?",
                        selected_.title + "\nVérification SHA-256 obligatoire. Limite : 2 Gio. "
                        "Le fichier sera téléchargé, sans installation automatique.",
                        {{"Annuler", ui::ButtonKind::secondary, false}, {"Télécharger", ui::ButtonKind::primary, false}}, 0}, feedback);
                }
            }
        }
        else if (edit_step_ >= 0)
        {
            keys_.set_length(static_cast<int>(edit_.size()));
            const auto event = keys_.handle(input, feedback);
            if (event == ui::Event::cancelled)
            {
                edit_step_ = -1;
                edit_.clear();
            }
            if (event == ui::Event::activated)
            {
                if (edit_step_ == 3)
                {
                    query_ = edit_;
                    edit_step_ = -1;
                    cover_key_.clear();
                    grid_.set_focus(0);
                    refresh();
                }
                else
                    accept_field(feedback);
            }
        }
        else if (input.is_pressed(Action::page_prev) && section_ <= 3)
        {
            edit_ = query_;
            edit_step_ = 3;
            edit_error_.clear();
            keys_.style.max_length = 80;
            keys_.enter();
        }
        else if (input.is_pressed(Action::page_next) && section_ <= 3)
        {
            filter_ = (filter_ + 1) % 3;
            grid_.set_focus(0);
            cover_key_.clear();
            refresh();
        }
        else if (input.is_pressed(Action::menu) && section_ <= 3 && !indices_.empty())
            toggle_favorite(snapshot_.entries[indices_[static_cast<std::size_t>(content_focus())]], feedback);
        else if (input.is_pressed(Action::west))
        {
            if (snapshot_.busy)
                service_.cancel();
            else
            {
                cover_key_.clear();
                service_.load();
                sync_pending_ = section_ != 4;
            }
        }
        else if (input.is_pressed(Action::north) &&
                 (section_ == 1 || section_ == 2 || section_ == 3) && !snapshot_.busy)
        {
            draft_ = {};
            edit_step_ = 0;
            edit_.clear();
            keys_.style.max_length = 120;
            keys_.enter();
            feedback.play(audio::Cue::select);
        }
        else if (input.is_pressed(Action::back) ||
                 (input.nav == Direction::left &&
                  (!is_grid() || sidebar_ || grid_.focus() % grid_.columns() == 0)))
        {
            sidebar_ = true;
            feedback.play(audio::Cue::back);
        }
        else if (sidebar_)
        {
            const auto event = nav_.handle(input, feedback);
            if (nav_.focus() != section_)
            {
                section_ = nav_.focus();
                refresh();
            }
            if (event == ui::Event::activated || input.nav == Direction::right)
            {
                sidebar_ = false;
                feedback.play(audio::Cue::select);
            }
        }
        else if ((is_grid() ? grid_.handle(input, feedback) : items_.handle(input, feedback)) ==
                 ui::Event::activated)
            activate(feedback);
        nav_.set_active(sidebar_ && edit_step_ < 0 && !dialog_.is_open() && !detail_open_);
        items_.set_active(!sidebar_ && edit_step_ < 0 && !dialog_.is_open() && !detail_open_);
        grid_.set_active(!sidebar_ && edit_step_ < 0 && !dialog_.is_open() && !detail_open_);
        request_cover_page();
        grid_.update(dt);
        nav_.update(dt);
        items_.update(dt);
        dialog_.update(dt);
        keys_.update(dt);
    }
    void draw(app::Frame &frame) const override
    {
        frame.backdrop.mode = gfx::BackdropMode::aurora;
        frame.backdrop.colors[0] = Color::rgb(0x070d18);
        frame.backdrop.colors[1] = Color::rgb(0x112339);
        frame.backdrop.colors[2] = Color::rgb(0x173c49);
        frame.backdrop.colors[3] = Color::rgb(0x29224b);
        frame.backdrop.time = clock_ * 0.2f;
        auto &list = frame.scene;
        const auto &fonts = context_.fonts;
        list.rounded_rect({96, 76, 60, 60}, 18, theme_.primary);
        ui::text(list, fonts.semibold, "H", 126, 119, 40, theme_.on_primary, gfx::Align::center);
        ui::text(list, fonts.semibold, "PS5 Hub", 176, 119, 38, theme_.text);
        ui::text(list, fonts.regular, "COMMUNAUTÉ  /  0.3 ALPHA", 1824, 113, 20,
                 theme_.text_muted, gfx::Align::right);
        if (section_ == 0 && edit_step_ < 0 && !indices_.empty())
        {
            const auto &featured = snapshot_.entries[indices_[static_cast<std::size_t>(content_focus())]];
            list.rounded_rect({472, 210, 1352, 250}, 24, Color::rgb(0x142b3d));
            auto texture = textures_.find(featured.id);
            if (texture != textures_.end())
                list.image(texture->second, {1554, 210, 168, 250}, gfx::kFullUv, Color::rgb(0xffffff), 16);
            ui::text(list, fonts.semibold, "À DÉCOUVRIR  /  NATIF PS5", 510, 261, 18, theme_.primary);
            ui::text(list, fonts.display, fonts.display.font->fit(featured.title, 46, 990), 510, 327, 46, theme_.text);
            ui::text(list, fonts.regular, "Jeux et portages de la communauté, liens des développeurs.", 512, 376, 23, theme_.text_muted);
            ui::text(list, fonts.semibold, "Croix : voir la fiche", 512, 427, 22, theme_.primary);
        }
        else
        {
            list.rounded_rect({472, 230, 1352, 170}, 24, Color::rgb(0x172c3f, 0.85f));
            ui::text(list, fonts.display, sections[section_], 504, 301, 54, theme_.text);
            const auto subtitle = section_ <= 3 ? (query_.empty() ? std::string(subtitles[section_]) : "Recherche : " + query_) : std::string(subtitles[section_]);
            ui::text(list, fonts.regular, fonts.regular.font->fit(subtitle, 24, 1220), 506, 354, 24, theme_.text_muted);
        }
        ui::text(list, fonts.semibold, "VOTRE ESPACE", 110, 235, 18, theme_.text_muted);
        ui::Canvas canvas{list, fonts, 0, clock_};
        nav_.draw(canvas);
        if (edit_step_ >= 0)
        {
            const char *labels[] = {"1 / 3 — Nom du contenu", "2 / 3 — Lien HTTPS direct",
                                    "3 / 3 — SHA-256 de l’auteur (optionnel)", "Rechercher dans les titres et les fiches"};
            ui::text(list, fonts.semibold, labels[edit_step_], 504, 430, 28, theme_.text);
            keys_.draw(canvas);
            const auto shown = fonts.regular.font->fit(edit_, 25, 1256);
            ui::text(list, fonts.regular, shown.empty() ? "Saisissez votre texte…" : shown, 520,
                     912, 25, theme_.primary);
            ui::text(list, fonts.regular, edit_error_, 520, 950, 22, Color::rgb(0xf4c787));
        }
        else
        {
            if (is_grid())
            {
                const char *filters[] = {"Tous", "Favoris", "Archives"};
                ui::text(list, fonts.semibold, std::to_string(indices_.size()) + " résultats  ·  " + filters[filter_], 510, section_ == 0 ? 491 : 426, 19, theme_.text_muted);
                grid_.draw(canvas);
            }
            else
                items_.draw(canvas);
            const auto message = fonts.regular.font->fit(snapshot_.message, 23, 1280);
            ui::text(list, fonts.regular, message, 504, 900, 23, theme_.primary);
            if (snapshot_.busy && service_.bytes() > 0)
                ui::text(list, fonts.mono, std::to_string(service_.bytes() / 1024) + " Kio reçus",
                         504, 940, 22, theme_.text_muted);
        }
        ui::text(list, fonts.regular, "13.60", 110, 902, 26, theme_.text);
        ui::text(list, fonts.regular, "Démarrage signalé · 13.60", 110, 940, 22,
                 Color::rgb(0xf4c787));
        const ui::Hint hints[] = {
            {ui::Button::cross, edit_step_ >= 0 ? "Saisir" : "Ouvrir"},
            {ui::Button::circle, "Retour"},
            {ui::Button::triangle, edit_step_ >= 0 ? "Espace" : "Ajouter"},
            {ui::Button::square, edit_step_ >= 0  ? "Effacer"
                                 : snapshot_.busy ? "Annuler"
                                                  : "Actualiser"}};
        if (!detail_open_)
            ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), hints,
                           (edit_step_ >= 0 || section_ <= 3) ? 4 : 2, 1824, true);
        else
        {
            const ui::Hint detail_hints[] = {{ui::Button::cross, hub::is_reference(selected_) ? "Projet" : "Télécharger"},
                {ui::Button::circle, "Fermer"}, {ui::Button::options, "Favori"}};
            ui::draw_hints(list, fonts, ui::GlyphStyle::dark(), detail_hints, 3, 1824, true);
        }
        if (edit_step_ < 0 && section_ <= 3 && !detail_open_)
            ui::text(list, fonts.regular, "L1 Rechercher   R1 Filtrer   Options Favori", 504, 963, 20, theme_.text_muted);
        if (detail_open_)
            draw_detail(list, fonts);
        frame.glass = dialog_.visible();
        ui::Canvas overlay{frame.overlay, fonts, frame.glass_texture, clock_};
        dialog_.draw(overlay);
    }
    std::span<const app::TourStep> tour() const override
    {
        static const app::TourStep steps[] = {
            {1.0f, 0, Direction::none, "accueil"},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "jeux"},
            {0.2f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.2f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.8f, 0, Direction::none, "fiche-jeu"},
            {0.2f, action_bit(Action::confirm), Direction::none, nullptr},
            {0.8f, 0, Direction::none, "confirmation"},
            {0.2f, action_bit(Action::back), Direction::none, nullptr},
            {0.2f, action_bit(Action::back), Direction::none, nullptr},
            {0.2f, action_bit(Action::page_prev), Direction::none, nullptr},
            {0.8f, 0, Direction::none, "recherche"},
            {0.2f, action_bit(Action::back), Direction::none, nullptr},
            {0.2f, action_bit(Action::back), Direction::none, nullptr},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "homebrew"},
            {0.3f, 0, Direction::down, nullptr},
            {0.2f, action_bit(Action::north), Direction::none, nullptr},
            {0.8f, 0, Direction::none, "ajout"},
            {0.3f, action_bit(Action::back), Direction::none, nullptr},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "fichiers"},
            {0.3f, 0, Direction::down, nullptr},
            {0.8f, 0, Direction::none, "parametres"}};
        return steps;
    }

  private:
    std::string project_page(const hub::Entry &entry) const
    {
        if (entry.url.starts_with("https://github.com/"))
        {
            const auto owner = entry.url.find('/', 19);
            if (owner != std::string::npos)
            {
                const auto end = entry.url.find('/', owner + 1);
                return entry.url.substr(0, end);
            }
        }
        return entry.url;
    }
    void open_detail(const hub::Entry &entry)
    {
        selected_ = entry;
        detail_open_ = true;
        std::uint8_t scratch[qrcodegen_BUFFER_LEN_MAX], code[qrcodegen_BUFFER_LEN_MAX];
        if (!qrcodegen_encodeText(project_page(entry).c_str(), scratch, code,
            qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true))
        {
            qr_size_ = 0;
            qr_.clear();
            return;
        }
        qr_size_ = qrcodegen_getSize(code);
        qr_.assign(static_cast<std::size_t>(qr_size_ * qr_size_), 0);
        for (int y = 0; y < qr_size_; ++y)
            for (int x = 0; x < qr_size_; ++x)
                qr_[static_cast<std::size_t>(y * qr_size_ + x)] = qrcodegen_getModule(code, x, y);
    }
    void toggle_favorite(const hub::Entry &entry, app::Feedback &feedback)
    {
        auto next = favorites_;
        if (!next.erase(entry.id))
            next.insert(entry.id);
        std::string text;
        for (const auto &id : next)
            text += id + '\n';
        const auto error = hui::save::write_atomic(std::string(data_root) + "/favorites.txt", text);
        if (!error.empty())
        {
            notice("Favoris non enregistrés", error, feedback);
            return;
        }
        favorites_ = std::move(next);
        refresh();
        cover_key_.clear();
        feedback.play(audio::Cue::select);
    }
    void draw_detail(gfx::DrawList &list, const ui::Fonts &fonts) const
    {
        list.rounded_rect({450, 190, 1390, 770}, 28, Color::rgb(0x101c2d));
        list.gradient_rect_h({450, 190, 1390, 770}, 28, Color::rgb(0x15323f, 0.55f), Color::rgb(0x101624, 0.2f));
        auto texture = textures_.find(selected_.id);
        if (texture != textures_.end())
            list.image(texture->second, {500, 280, 256, 384}, gfx::kFullUv, Color::rgb(0xffffff), 18);
        ui::text(list, fonts.semibold, hub::is_game(selected_) ? "JEU / PORTAGE PS5" : "APPLICATION PS5", 800, 260, 19, theme_.primary);
        ui::paragraph(list, fonts.display, selected_.title, 800, 320, 39, 665, 48, theme_.text, 2);
        ui::paragraph(list, fonts.regular, selected_.description, 800, 437, 24, 665, 34, theme_.text_muted, 6);
        const bool reference = hub::is_reference(selected_);
        ui::text(list, fonts.semibold, reference ? "FICHE DE RÉFÉRENCE" : "ARCHIVE · EMPREINTE SHA-256 FOURNIE", 800, 677, 19, theme_.primary);
        ui::paragraph(list, fonts.regular, reference ? "Consultez le projet sur votre téléphone." :
            "Archive du développeur. Installation manuelle ; limite de téléchargement : 2 Gio.", 800, 716, 22, 665, 30, theme_.text_muted, 2);
        list.rounded_rect({800, 794, 630, 62}, 16, theme_.primary);
        ui::text(list, fonts.semibold, reference ? "Scannez le QR pour consulter le projet" : "Croix : télécharger l’archive", 826, 835, 24, theme_.on_primary);
        const auto label = favorites_.count(selected_.id) ? "Options : retirer des favoris" : "Options : ajouter aux favoris";
        ui::text(list, fonts.regular, label, 800, 902, 22, theme_.text_muted);
        if (qr_size_ > 0)
        {
            const float module = std::max(1.0f, std::floor(256.0f / (qr_size_ + 8)));
            const float side = module * (qr_size_ + 8);
            const float left = 1514 + (276 - side) * 0.5f;
            list.rounded_rect({left, 320, side, side}, 0, Color::rgb(0xffffff));
            for (int y = 0; y < qr_size_; ++y)
                for (int x = 0; x < qr_size_; ++x)
                    if (qr_[static_cast<std::size_t>(y * qr_size_ + x)])
                        list.rounded_rect({left + (x + 4) * module, 320 + (y + 4) * module, module, module}, 0, Color::rgb(0x000000));
            ui::paragraph(list, fonts.regular, "Scannez avec votre téléphone\nPage du développeur", 1514, 620, 21, 276, 31, theme_.text_muted, 3);
        }
        ui::text(list, fonts.regular, "Cercle : fermer la fiche", 500, 909, 22, theme_.text_muted);
    }
    void notice(std::string title, std::string body, app::Feedback &feedback)
    {
        pending_download_ = false;
        dialog_.open({ui::StatusKind::none,
                      std::move(title),
                      std::move(body),
                      {{"Fermer", ui::ButtonKind::secondary, false}},
                      0},
                     feedback);
    }
    bool is_grid() const
    {
        return section_ <= 3;
    }
    int content_focus() const
    {
        return is_grid() ? grid_.focus() : items_.focus();
    }
    void release_images()
    {
        if (context_.image_release)
            for (const auto &pair : textures_)
                context_.image_release(pair.second);
        textures_.clear();
    }
    void upload_images()
    {
        if (snapshot_.artwork_revision == art_revision_ || !context_.image_upload)
            return;
        release_images();
        for (const auto &pair : snapshot_.covers)
        {
            const auto &art = *pair.second;
            textures_[pair.first] = context_.image_upload(art.width, art.height, art.pixels.data());
        }
        art_revision_ = snapshot_.artwork_revision;
    }
    ui::CardItem card(std::string title, std::string subtitle, std::string badge,
                      const std::string &image) const
    {
        ui::CardItem item;
        item.title = std::move(title);
        item.subtitle = std::move(subtitle);
        item.badge = std::move(badge);
        auto texture = textures_.find(image);
        if (texture != textures_.end())
        {
            item.texture = texture->second;
            const auto &art = *snapshot_.covers.at(image);
            item.image_aspect = static_cast<float>(art.width) / art.height;
        }
        item.top = Color::rgb(0x245767);
        item.bottom = Color::rgb(0x17243d);
        item.accent = theme_.primary;
        return item;
    }
    void request_cover_page()
    {
        if (snapshot_.busy)
            return;
        std::vector<std::string> ids;
        std::string key = std::to_string(section_);
        const auto start = static_cast<std::size_t>(std::max(content_focus(), 0) / 10 * 10);
        for (auto i = start; i < indices_.size() && i < start + 10; ++i)
        {
            const auto &id = snapshot_.entries[indices_[i]].id;
            ids.push_back(id);
            key += id;
        }
        if (key != cover_key_ && service_.request_covers(std::move(ids)))
            cover_key_ = key;
    }
    void refresh()
    {
        items_.style.row_height = section_ == 5 ? 88.0f : 104.0f;
        std::vector<ui::ListItem> rows;
        std::vector<ui::CardItem> cards;
        indices_.clear();
        grid_.style.columns = 5;
        grid_.style.cell_height = section_ == 0 ? 320 : 375;
        grid_.style.card.art_aspect = section_ == 0 ? 1.0f : 0.8f;
        grid_.style.card.title_size = 23;
        grid_.style.card.subtitle_size = 20;
        if (section_ == 0)
        {
            // Le premier lancement expose de vrais jeux/portages, avant tout import privé.
        }
        if (section_ <= 3)
        {
            for (std::size_t i = 0; i < snapshot_.entries.size(); ++i)
            {
                const auto &entry = snapshot_.entries[i];
                const bool visible = section_ <= 1 ? hub::is_game(entry)
                                     : section_ == 2 ? !hub::is_game(entry)
                                                     : entry.kind == "homebrew";
                if (!visible || !hub::matches(entry, query_) ||
                    (filter_ == 1 && !favorites_.count(entry.id)) ||
                    (filter_ == 2 && (hub::is_reference(entry) || entry.sha256.empty())))
                    continue;
                auto image = textures_.count(entry.id)
                                 ? entry.id
                                 : (section_ == 1 ? "hub-feature" : "hub-homebrew");
                cards.push_back(card(entry.title,
                                     favorites_.count(entry.id) ? "Dans vos favoris" : entry.description,
                                     entry.kind == "pkg" ? "PKG PERSONNEL" : hub::is_reference(entry) ? "PROJET" : "ARCHIVE",
                                     image));
                indices_.push_back(i);
            }
            if (cards.empty())
                cards.push_back(card("Aucun résultat", "L1 : rechercher · R1 : changer de filtre",
                                     "CATALOGUE", "hub-feature"));
        }
        else if (section_ == 4)
        {
            for (const auto &name : snapshot_.files)
                rows.push_back(row(name, "Stockage personnel / downloads", "LOCAL"));
            if (rows.empty())
                rows.push_back(row("Aucun téléchargement",
                                   "Les gros PKG sont préparés et transférés par le bot PC."));
        }
        else
        {
            rows.push_back(row("Réduire les animations", "Préférence enregistrée automatiquement",
                               context_.settings.reduced_motion ? "OUI" : "NON"));
            rows.push_back(row("Effets sonores", "Sons de navigation",
                               context_.settings.ui_volume ? "OUI" : "NON"));
            rows.push_back(row("Vérifier les mises à jour", "Version publiée de PS5 Hub", "HTTPS"));
            rows.push_back(row("Bot PC et transfert PKG",
                               "Import en masse, jaquettes, FTP et HyperDPI", "0.3 ALPHA"));
            rows.push_back(row("Actualiser le catalogue partagé", "Conserve vos imports et vos favoris", "HTTPS"));
        }
        const int focus = content_focus();
        items_.set_items(std::move(rows));
        items_.set_focus(focus);
        grid_.set_items(std::move(cards));
        grid_.set_bounds(section_ == 0 ? gfx::Rect{504, 500, 1320, 385} : gfx::Rect{504, 450, 1320, 420});
        grid_.set_focus(focus);
    }
    void activate(app::Feedback &feedback)
    {
        if (section_ <= 3 && !indices_.empty())
        {
            open_detail(snapshot_.entries[indices_[static_cast<std::size_t>(content_focus())]]);
            return;
        }
        if (section_ == 5)
        {
            switch (items_.focus())
            {
            case 0:
                context_.settings.reduced_motion = !context_.settings.reduced_motion;
                context_.settings_changed = true;
                refresh();
                break;
            case 1:
                context_.settings.ui_volume = context_.settings.ui_volume ? 0 : 7;
                context_.settings_changed = true;
                refresh();
                break;
            case 2:
                if (!service_.check_update())
                    notice("Opération en cours",
                           "Attendez la fin du travail actuel ou annulez-le avec Carré.", feedback);
                break;
            case 4:
                service_.sync_catalog();
                break;
            default:
                notice("PS5 Hub — version alpha",
                       "GPL-3.0-or-later. Base BlackBearReloaded. Firmware 13.60, HyperHEN, "
                       "DualSense et ShadowMountPlus : validation matérielle à effectuer.",
                       feedback);
                break;
            }
        }
        else if (section_ == 4)
            notice(
                "Fichiers téléchargés",
                "Consultation des téléchargements du hub. Installation système indisponible : "
                "aucun connecteur validé pour votre configuration. Consultez le guide USB / FTP.",
                feedback);
        else
            notice("Importer vos jeux en une fois",
                   "Sur Windows, lancez le bot PS5 Hub. Collez votre liste de liens, préparez les "
                   "jaquettes puis synchronisez par FTP. Carré recharge la bibliothèque. Pour les "
                   "gros PKG, le bot peut transférer vers FTP ou HyperDPI.",
                   feedback);
    }
    void accept_field(app::Feedback &feedback)
    {
        edit_error_.clear();
        if (edit_step_ == 0)
        {
            if (edit_.empty())
            {
                edit_error_ = "Indiquez un nom pour ce contenu.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.title = edit_;
            edit_.clear();
            edit_step_ = 1;
            keys_.style.max_length = 2048;
        }
        else if (edit_step_ == 1)
        {
            if (!hub::valid_url(edit_))
            {
                edit_error_ = "Saisissez une adresse commençant par https://, sans espace.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.url = edit_;
            edit_.clear();
            edit_step_ = 2;
            keys_.style.max_length = 64;
        }
        else
        {
            if (!edit_.empty() && !hub::valid_hash(edit_))
            {
                edit_error_ = "Le SHA-256 doit contenir 64 caractères : 0 à 9 et a à f.";
                feedback.play(audio::Cue::error);
                return;
            }
            draft_.sha256 = edit_;
            draft_.kind = section_ == 1 ? "pkg" : "homebrew";
            draft_.version = "00.000.000";
            draft_.description = "Lien personnel — compatibilité à vérifier";
            draft_.id = "perso-" + std::to_string(static_cast<long long>(std::time(nullptr))) +
                        "-" + std::to_string(snapshot_.entries.size());
            service_.add(draft_);
            edit_step_ = -1;
            edit_.clear();
            feedback.play(audio::Cue::complete);
        }
        keys_.set_length(0);
    }
    app::Context &context_;
    hub::Service service_;
    hub::Snapshot snapshot_;
    ui::Theme theme_;
    ui::ListView nav_, items_;
    ui::GridView grid_;
    std::map<std::string, std::uint32_t> textures_;
    std::string cover_key_;
    unsigned art_revision_ = 0;
    ui::Dialog dialog_;
    ui::Keyboard keys_;
    std::vector<std::size_t> indices_;
    hub::Entry selected_, draft_;
    std::string edit_, edit_error_;
    std::string query_;
    std::set<std::string> favorites_;
    std::vector<std::uint8_t> qr_;
    int qr_size_ = 0, filter_ = 0;
    bool detail_open_ = false, sync_pending_ = false;
    int section_ = 0, edit_step_ = -1;
    bool sidebar_ = true, pending_download_ = false;
    float clock_ = 0;
};
} // namespace
std::unique_ptr<app::Concept> make_hub(app::Context &c)
{
    return std::make_unique<Hub>(c);
}
} // namespace hui::concepts
