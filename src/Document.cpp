#include "Document.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

#include <BRepBuilderAPI_Transform.hxx>
#include <BRepTools.hxx>
#include <BRep_Builder.hxx>
#include <utility>

namespace
{
constexpr std::size_t kTailleMaxHistorique = 100;

bool contient(const std::vector<int>& ids, int id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
} // namespace

int Document::ajouter(const TopoDS_Shape& forme, const std::string& nomBase,
                      std::shared_ptr<const Parametrique> parametres)
{
    memoriser();

    Piece piece;
    piece.id    = m_prochainId++;
    piece.nom   = nomBase + " " + std::to_string(piece.id);
    piece.forme = forme;
    piece.parametres = std::move(parametres);
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
        {
            piece.forme = operation(piece.forme);
            piece.parametres.reset(); // la forme ne correspond plus à la définition
            piece.placement = gp_Trsf();
        }
    }
}

void Document::transformer(const std::vector<int>& ids,
                           const std::function<gp_Trsf(const TopoDS_Shape&)>& calcul)
{
    memoriser();

    for (Piece& piece : m_pieces)
    {
        if (!contient(ids, piece.id))
            continue;
        const gp_Trsf t = calcul(piece.forme);
        piece.forme     = BRepBuilderAPI_Transform(piece.forme, t, true).Shape();
        piece.placement = t.Multiplied(piece.placement);
    }
}

void Document::redefinirParametres(int id, const TopoDS_Shape& forme,
                                   std::shared_ptr<const Parametrique> parametres)
{
    memoriser();

    for (Piece& piece : m_pieces)
    {
        if (piece.id == id)
        {
            piece.forme      = forme;
            piece.parametres = std::move(parametres);
        }
    }
}

void Document::composerOrientation(const std::vector<int>& ids, const gp_Quaternion& rotation)
{
    for (Piece& piece : m_pieces)
    {
        if (contient(ids, piece.id))
            piece.orientation = rotation.Multiplied(piece.orientation);
    }
}

void Document::remplacer(int idConservee, const TopoDS_Shape& forme,
                         const std::vector<int>& idsSupprimees)
{
    memoriser();

    m_pieces.erase(std::remove_if(m_pieces.begin(), m_pieces.end(),
                                  [&](const Piece& piece) {
                                      return piece.id != idConservee && contient(idsSupprimees, piece.id);
                                  }),
                   m_pieces.end());

    for (Piece& piece : m_pieces)
    {
        if (piece.id == idConservee)
        {
            piece.forme = forme;
            piece.parametres.reset();
            piece.placement = gp_Trsf();
        }
    }
}

bool Document::enregistrer(const std::string& chemin) const
{
    std::ofstream fichier(chemin, std::ios::binary);
    if (!fichier)
        return false;

    fichier << "FREETABLE3D 2\n";
    fichier << "PROCHAIN_ID " << m_prochainId << "\n";
    fichier << "PIECES " << m_pieces.size() << "\n";
    fichier << std::setprecision(17);

    for (const Piece& piece : m_pieces)
    {
        std::ostringstream brep;
        brep << std::setprecision(17);
        BRepTools::Write(piece.forme, brep);
        const std::string contenu = brep.str();

        std::string nom = piece.nom;
        std::replace(nom.begin(), nom.end(), '\n', ' ');
        std::replace(nom.begin(), nom.end(), '\r', ' ');

        fichier << "PIECE " << piece.id << ' '
                << piece.orientation.X() << ' ' << piece.orientation.Y() << ' '
                << piece.orientation.Z() << ' ' << piece.orientation.W() << ' '
                << contenu.size() << "\n";
        fichier << nom << "\n";
        fichier.write(contenu.data(), static_cast<std::streamsize>(contenu.size()));
        fichier << "\n";

        // Définition paramétrique éventuelle
        if (!piece.parametres)
        {
            fichier << "PARAM 0\n";
        }
        else
        {
            const Parametrique& p = *piece.parametres;
            const Esquisse& e = p.esquisse;
            fichier << "PARAM 1 " << p.hauteur << ' ' << e.origine.X() << ' ' << e.origine.Y() << ' '
                    << e.origine.Z() << ' ' << e.normale.X() << ' ' << e.normale.Y() << ' '
                    << e.normale.Z() << ' ' << e.axeU.X() << ' ' << e.axeU.Y() << ' ' << e.axeU.Z()
                    << ' ' << e.profils.size() << "\n";
            for (const Profil& profil : e.profils)
            {
                fichier << "PROFIL " << static_cast<int>(profil.type) << ' ' << profil.v.size();
                for (double valeur : profil.v)
                    fichier << ' ' << valeur;
                fichier << "\n";
            }
            fichier << "PLACEMENT";
            for (int ligne = 1; ligne <= 3; ++ligne)
                for (int colonne = 1; colonne <= 4; ++colonne)
                    fichier << ' ' << piece.placement.Value(ligne, colonne);
            fichier << "\n";
        }
    }
    fichier.flush();
    return static_cast<bool>(fichier);
}

bool Document::charger(const std::string& chemin)
{
    std::ifstream fichier(chemin, std::ios::binary);
    if (!fichier)
        return false;

    std::string ligne;
    if (!std::getline(fichier, ligne)
        || (ligne.rfind("FREETABLE3D", 0) != 0 && ligne.rfind("CAOLIBRE", 0) != 0)) // ancien nom accepté
        return false;
    const bool avecParametres = ligne.rfind("FREETABLE3D 2", 0) == 0 || ligne.rfind("CAOLIBRE 2", 0) == 0;

    std::string mot;
    int prochainId = 1;
    std::size_t nombre = 0;

    if (!std::getline(fichier, ligne))
        return false;
    {
        std::istringstream entete(ligne);
        if (!(entete >> mot >> prochainId) || mot != "PROCHAIN_ID")
            return false;
    }
    if (!std::getline(fichier, ligne))
        return false;
    {
        std::istringstream entete(ligne);
        if (!(entete >> mot >> nombre) || mot != "PIECES")
            return false;
    }

    std::vector<Piece> pieces;
    for (std::size_t i = 0; i < nombre; ++i)
    {
        if (!std::getline(fichier, ligne))
            return false;

        Piece piece;
        double qx = 0, qy = 0, qz = 0, qw = 1;
        std::size_t taille = 0;
        {
            std::istringstream entete(ligne);
            if (!(entete >> mot >> piece.id >> qx >> qy >> qz >> qw >> taille) || mot != "PIECE")
                return false;
        }
        piece.orientation = gp_Quaternion(qx, qy, qz, qw);

        if (!std::getline(fichier, piece.nom))
            return false;

        std::string contenu(taille, '\0');
        fichier.read(&contenu[0], static_cast<std::streamsize>(taille));
        if (!fichier)
            return false;
        fichier.get(); // saut de ligne final

        if (avecParametres)
        {
            if (!std::getline(fichier, ligne))
                return false;
            std::istringstream entete(ligne);
            int present = 0;
            if (!(entete >> mot >> present) || mot != "PARAM")
                return false;

            if (present)
            {
                auto p = std::make_shared<Parametrique>();
                double ox, oy, oz, nx, ny, nz, ux, uy, uz;
                std::size_t nbProfils = 0;
                if (!(entete >> p->hauteur >> ox >> oy >> oz >> nx >> ny >> nz >> ux >> uy >> uz >> nbProfils))
                    return false;
                p->esquisse.origine = gp_Pnt(ox, oy, oz);
                p->esquisse.normale = gp_Dir(nx, ny, nz);
                p->esquisse.axeU    = gp_Dir(ux, uy, uz);

                for (std::size_t k = 0; k < nbProfils; ++k)
                {
                    if (!std::getline(fichier, ligne))
                        return false;
                    std::istringstream flux(ligne);
                    int type = 0;
                    std::size_t n = 0;
                    if (!(flux >> mot >> type >> n) || mot != "PROFIL")
                        return false;
                    Profil profil;
                    profil.type = static_cast<TypeProfil>(type);
                    profil.v.resize(n);
                    for (double& valeur : profil.v)
                        if (!(flux >> valeur))
                            return false;
                    p->esquisse.profils.push_back(profil);
                }

                if (!std::getline(fichier, ligne))
                    return false;
                std::istringstream flux(ligne);
                double m[12];
                if (!(flux >> mot) || mot != "PLACEMENT")
                    return false;
                for (double& valeur : m)
                    if (!(flux >> valeur))
                        return false;
                piece.placement.SetValues(m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7],
                                          m[8], m[9], m[10], m[11]);
                piece.parametres = p;
            }
        }

        std::istringstream brep(contenu);
        BRep_Builder constructeur;
        TopoDS_Shape forme;
        BRepTools::Read(forme, brep, constructeur);
        if (forme.IsNull())
            return false;

        piece.forme = forme;
        pieces.push_back(piece);
    }

    m_pieces = std::move(pieces);
    m_prochainId = std::max(prochainId, 1);
    for (const Piece& piece : m_pieces)
        m_prochainId = std::max(m_prochainId, piece.id + 1);
    m_pileAnnuler.clear();
    m_pileRefaire.clear();
    return true;
}

std::vector<int> Document::dupliquer(const std::vector<int>& ids,
                                     const std::vector<gp_Trsf>& transformations,
                                     bool conserverParametres)
{
    memoriser();

    std::vector<int>   nouveaux;
    std::vector<Piece> copies;
    for (const Piece& original : m_pieces)
    {
        if (!contient(ids, original.id))
            continue;

        for (const gp_Trsf& t : transformations)
        {
            Piece copie  = original;
            copie.id     = m_prochainId++;
            copie.nom    = original.nom + " (copie)";
            copie.forme  = BRepBuilderAPI_Transform(original.forme, t, true).Shape();
            if (conserverParametres)
            {
                copie.placement = t.Multiplied(original.placement);
            }
            else
            {
                copie.parametres.reset();
                copie.placement = gp_Trsf();
            }
            nouveaux.push_back(copie.id);
            copies.push_back(copie);
        }
    }
    m_pieces.insert(m_pieces.end(), copies.begin(), copies.end());
    return nouveaux;
}

std::vector<int> Document::coller(const std::vector<Piece>& modeles, const gp_Trsf& t)
{
    if (modeles.empty())
        return {};
    memoriser();

    const std::string suffixe = " (copie)";
    std::vector<int>  nouveaux;
    for (const Piece& modele : modeles)
    {
        Piece copie     = modele;
        copie.id        = m_prochainId++;
        const bool deja = copie.nom.size() >= suffixe.size()
                          && copie.nom.compare(copie.nom.size() - suffixe.size(), suffixe.size(), suffixe) == 0;
        if (!deja)
            copie.nom += suffixe;
        copie.forme = BRepBuilderAPI_Transform(modele.forme, t, true).Shape();
        if (modele.parametres)
            copie.placement = t.Multiplied(modele.placement);
        nouveaux.push_back(copie.id);
        m_pieces.push_back(copie);
    }
    return nouveaux;
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
