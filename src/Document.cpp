#include "Document.h"

#include <algorithm>
#include <utility>

namespace
{
constexpr std::size_t kTailleMaxHistorique = 100;

bool contient(const std::vector<int>& ids, int id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
} // namespace

int Document::ajouter(const TopoDS_Shape& forme, const std::string& nomBase)
{
    memoriser();

    Piece piece;
    piece.id    = m_prochainId++;
    piece.nom   = nomBase + " " + std::to_string(piece.id);
    piece.forme = forme;
    m_pieces.push_back(piece);
    return piece.id;
}

void Document::supprimer(const std::vector<int>& ids)
{
    memoriser();

    m_pieces.erase(std::remove_if(m_pieces.begin(), m_pieces.end(),
                                  [&ids](const Piece& piece) { return contient(ids, piece.id); }),
                   m_pieces.end());
}

void Document::modifier(const std::vector<int>& ids,
                        const std::function<TopoDS_Shape(const TopoDS_Shape&)>& operation)
{
    memoriser();

    for (Piece& piece : m_pieces)
    {
        if (contient(ids, piece.id))
            piece.forme = operation(piece.forme);
    }
}

void Document::vider()
{
    memoriser();
    m_pieces.clear();
}

const Piece* Document::trouver(int id) const
{
    for (const Piece& piece : m_pieces)
    {
        if (piece.id == id)
            return &piece;
    }
    return nullptr;
}

void Document::memoriser()
{
    m_pileAnnuler.push_back(m_pieces);
    if (m_pileAnnuler.size() > kTailleMaxHistorique)
        m_pileAnnuler.erase(m_pileAnnuler.begin());
    m_pileRefaire.clear();
}

void Document::annuler()
{
    if (m_pileAnnuler.empty())
        return;

    m_pileRefaire.push_back(m_pieces);
    m_pieces = std::move(m_pileAnnuler.back());
    m_pileAnnuler.pop_back();
}

void Document::refaire()
{
    if (m_pileRefaire.empty())
        return;

    m_pileAnnuler.push_back(m_pieces);
    m_pieces = std::move(m_pileRefaire.back());
    m_pileRefaire.pop_back();
}
