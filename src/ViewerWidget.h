#pragma once

// Widget Qt qui affiche le document en 3D avec OpenCascade.
//
// Souris :
//  - clic gauche : sélection ; Maj+clic : ajouter/retirer de la sélection
//  - glisser sur le vide ou une pièce non sélectionnée : rotation de la vue
//  - glisser une PIÈCE sélectionnée (mode Pièce) : la déplacer sur la grille
//  - glisser une FACE PLANE sélectionnée (mode Face) : l'étirer ou la rentrer
//  - clic droit ou milieu + glisser : déplacer la vue ; molette : zoom

#include <functional>
#include <map>
#include <vector>

#include <QElapsedTimer>
#include <QKeyEvent>
#include <QLabel>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

#include <AIS_InteractiveContext.hxx>
#include <AIS_Point.hxx>
#include <AIS_Shape.hxx>
#include <AIS_TextLabel.hxx>
#include <Bnd_Box.hxx>
#include <Quantity_Color.hxx>
#include <SelectMgr_EntityOwner.hxx>
#include <TopoDS_Shape.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Vec.hxx>

#include "AncragesMesure.h"
#include "Document.h"
#include "Esquisse.h"
#include "CotesEsquisse.h"
#include "GrilleAxes.h"

// Ce que le clic sélectionne : la pièce entière, une de ses faces ou une de ses arêtes.
enum class ModeSelection
{
    Piece,
    Face,
    Arete
};

// Axe de la rotation à la souris.
enum class AxeRotation
{
    X,
    Y,
    Z
};

// Outil de dessin d'une esquisse.
enum class OutilEsquisse
{
    Rectangle, // deux coins opposés
    Cercle,    // centre puis un point du cercle
    Polygone   // sommets successifs ; Entrée ou clic sur le premier sommet pour fermer
};

// Un élément actuellement sélectionné.
struct ElementSelectionne
{
    int          idPiece = 0;   // pièce à laquelle appartient l'élément
    TopoDS_Shape forme;         // la pièce, la face ou l'arête elle-même
    QString      description;   // ex. « Face plane · aire 400,00 mm² »
};

class ViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewerWidget(QWidget* parent = nullptr);

    // Reconstruit l'affichage à partir du document (la sélection est effacée).
    void afficherDocument(const Document& document);

    // Ajuste le zoom pour voir toutes les pièces (ou la zone de travail si le document est vide).
    void recadrer();

    // Oriente la caméra (dessus, face, côté, isométrique...) puis recadre.
    void orienterVue(V3d_TypeOfOrientation orientation);

    // Affiche ou masque la grille millimétrique et les axes.
    void afficherGrille(bool visible);

    // Affiche (ou masque) le volume d'impression : plateau X × Y centré sur l'origine, hauteur Z.
    void afficherPlateau(bool visible, double x, double y, double z);

    // Choisit ce que le clic sélectionne. Efface la sélection en cours.
    void definirModeSelection(ModeSelection mode);

    // Éléments actuellement sélectionnés.
    std::vector<ElementSelectionne> selection() const;

    // Sélectionne une pièce entière (mode Pièce uniquement). N'émet pas de signal.
    void selectionnerPiece(int idPiece);

    // Pas d'aimantation à la grille, en mm, pour le déplacement et l'étirement
    // à la souris (0 = libre).
    void definirAimantation(double pasMm);

    // Outil « rotation » : quand il est actif, glisser une pièce sélectionnée la fait
    // tourner (glisser vers la droite = angle positif) au lieu de la déplacer.
    void definirOutilRotation(bool actif);
    void definirAxeRotation(AxeRotation axe);
    // Pas d'aimantation des angles, en degrés (0 = libre).
    void definirAimantationAngle(double pasDegres);

    // Sélectionne plusieurs pièces entières (mode Pièce). N'émet pas de signal.
    void selectionnerPieces(const std::vector<int>& ids);

    // Repères rouges (petites sphères) sur les endroits où une paroi est trop fine.
    // « rayon » = taille des repères en mm. Ils disparaissent avec Échap ou à la prochaine modification.
    void afficherParoisFines(const std::vector<gp_Pnt>& points, double rayon);
    void effacerParoisFines();

    // --- Mesure de distance ---
    // Quand l'outil est actif, deux clics (sommet, arête ou face) donnent la distance entre les
    // deux points. Un troisième clic recommence une mesure ; Échap efface puis quitte.
    void definirOutilMesure(bool actif);
    bool outilMesureActif() const { return m_mesureActive; }

    // --- Esquisse 2D ---
    // Démarre une esquisse sur le plan passant par « origine » de normale « normale » :
    // la vue se place face au plan, et les clics ajoutent des points aimantés dans ce plan.
    void demarrerEsquisse(const gp_Pnt& origine, const gp_Dir& normale);
    void arreterEsquisse();
    bool esquisseActive() const { return m_esquisseActive; }
    void definirOutilEsquisse(OutilEsquisse outil);
    void effacerDernierProfil();
    // Profils fermés dessinés (faces planes) et normale du plan d'esquisse.
    std::vector<TopoDS_Shape> profilsEsquisse() const { return m_profils; }
    gp_Dir                    normaleEsquisse() const { return m_repere.Direction(); }
    // Esquisse paramétrique complète (plan + profils), pour créer une pièce modifiable.
    Esquisse esquisseCourante() const;

signals:
    // Émis quand l'utilisateur change la sélection à la souris.
    void selectionChangee();

    // Une mesure vient d'être faite (texte complet pour la barre d'état).
    void mesureEffectuee(const QString& texte);

    // L'outil de mesure s'est arrêté de lui-même (changement de mode, esquisse, Échap).
    void outilMesureArrete();

    // Une pièce (ou plusieurs) vient d'être déplacée à la souris de (dx, dy) mm.
    void deplacementTermine(const QVector<int>& ids, double dx, double dy);

    // Une pièce (ou plusieurs) vient d'être tournée à la souris : angles en degrés
    // autour des axes X, Y, Z (un seul est non nul).
    void rotationTerminee(const QVector<int>& ids, double rx, double ry, double rz);

    // Une face plane vient d'être étirée de « distance » mm (négative = rentrée).
    void etirementTermine(int idPiece, const TopoDS_Shape& face, double distance);

    // Texte à afficher pendant un geste (déplacement ou étirement en cours).
    void messageApercu(const QString& message);

    // Valeurs affichées près du curseur pendant un déplacement, une rotation ou un étirement
    // (texte sur plusieurs lignes ; vide = geste terminé, on masque l'affichage).
    void apercuGeste(const QString& texte);

protected:
    // OpenCascade dessine directement dans la fenêtre : Qt ne doit pas le faire.
    QPaintEngine* paintEngine() const override { return nullptr; }

    void paintEvent(QPaintEvent* evenement) override;
    void resizeEvent(QResizeEvent* evenement) override;
    void showEvent(QShowEvent* evenement) override;
    void moveEvent(QMoveEvent* evenement) override;
    void mousePressEvent(QMouseEvent* evenement) override;
    void mouseMoveEvent(QMouseEvent* evenement) override;
    void mouseReleaseEvent(QMouseEvent* evenement) override;
    void mouseDoubleClickEvent(QMouseEvent* evenement) override;
    void wheelEvent(QWheelEvent* evenement) override;
    void keyPressEvent(QKeyEvent* evenement) override;
    bool event(QEvent* evenement) override;
    void leaveEvent(QEvent* evenement) override;

private:
    QPoint versPixels(const QPointF& logique) const;

    struct ObjetAffiche
    {
        int               idPiece;
        Handle(AIS_Shape) objet;
    };

    // Geste en cours avec le bouton gauche.
    enum class Geste
    {
        Aucun,
        Rotation,
        Deplacement,
        Orientation,
        Etirement
    };

    // Resynchronise la vue OpenCascade avec la taille réelle de la fenêtre native.
    void ajusterVue();
    void synchroniserFenetreNative();
    void attacherFenetre();
    bool m_fenetreAttachee = false;
    void ajusterGrilleFine();

    // Numéro de mode de sélection OpenCascade correspondant au mode courant.
    int modeOcct() const;

    // Id de la pièce qui possède cet objet affiché (0 si inconnu).
    int idPieceDe(const Handle(AIS_InteractiveObject)& objet) const;

    // --- Géométrie sous le curseur ---
    // Rayon de vue passant par un point de l'écran.
    bool rayonSousCurseur(const QPoint& ecran, gp_Pnt& origine, gp_Vec& direction) const;
    // Intersection du rayon avec le plan du sol (Z = 0). Échoue si le plan est vu de profil.
    bool pointSurPlanSol(const QPoint& ecran, gp_Pnt& point) const;
    // Position, le long d'une droite (point + s * normale), du point le plus proche du rayon.
    bool parametreSurDroite(const QPoint& ecran, const gp_Pnt& point,
                            const gp_Vec& normale, double& s) const;
    // Arrondit une valeur au pas d'aimantation.
    double aimanter(double valeur) const;

    // --- Gestes ---
    // Retient les pièces sélectionnées (objets, identifiants, centres) et cumule leur boîte englobante.
    bool rassemblerSelection(Bnd_Box& boite);
    bool preparerDeplacement();
    bool preparerOrientation();
    void mettreAJourOrientation(const QPoint& ecran);
    double aimanterAngle(double degres) const;
    bool preparerEtirement(const Handle(SelectMgr_EntityOwner)& proprietaire);
    void mettreAJourDeplacement(const QPoint& ecran);
    void mettreAJourEtirement(const QPoint& ecran);
    void terminerGeste();
    void annoncerGeste(const QString& texte);

    // --- Esquisse ---
    bool pointSurEsquisse(const QPoint& ecran, gp_Pnt2d& uv) const;
    gp_Pnt point3D(const gp_Pnt2d& uv) const;
    TopoDS_Wire construireProfil(const std::vector<gp_Pnt2d>& points, const gp_Pnt2d* courant,
                                 bool ferme) const;
    void placerPointEsquisse(const QPoint& ecran);
    void mettreAJourApercu(const gp_Pnt2d& uv);
    void effacerApercu();
    void ajouterProfil(const TopoDS_Wire& contour, const Profil& description);
    void fermerPolygone();
    void mettreAJourAffichageEsquisse();
    void mettreAJourCotes();
    struct InfoCote
    {
        int    profil = 0;  // numéro du profil
        int    genre  = 0;  // 0 largeur, 1 hauteur (rectangle) ; 2 diamètre ; 3 côté de polygone
        int    indice = 0;  // numéro du côté (polygone)
        double valeur = 0.0;
        gp_Pnt position;    // emplacement du texte de la cote
        gp_Pnt debut;       // extrémités du trait de cote
        gp_Pnt fin;
    };
    bool         modifierCote(const InfoCote& info, double valeur);
    TopoDS_Shape faceDepuisProfil(const Profil& profil) const;
    void validerPointEsquisse(const gp_Pnt2d& uv);
    // Point défini par les cotes tapées au clavier (dans la direction de la souris).
    bool pointDepuisSaisie(gp_Pnt2d& uv) const;
    void rafraichirApercuSaisie();
    // Petite bulle d'information qui suit le curseur pendant le tracé.
    void afficherBulle(const QString& texte);
    void masquerBulle();
    void appliquerSurvol(const QPoint& position);
    // Désignation sous le curseur. Quand deux pièces affleurent (même profondeur), OpenCascade
    // choisit au hasard, donc toujours la plus grande : on préfère ici la plus petite.
    //  - preferSelection : si l'une des candidates est déjà sélectionnée, c'est elle (déplacement) ;
    //  - cycler : un clic sur une candidate déjà sélectionnée passe à la suivante (la plus grande).
    // Retourne l'élément retenu (nul s'il n'y en a pas).
    Handle(SelectMgr_EntityOwner) pointerCurseur(const QPoint& position, bool preferSelection = false,
                                                 bool cycler = false);
    double tailleProprietaire(const Handle(SelectMgr_EntityOwner)& proprietaire) const;
    bool   m_detectionSurchargee = false; // pointerCurseur a remplacé l'élément désigné par OpenCascade

    // --- Mesure ---
    void    activerModesMesure();
    void    arreterMesure();
    void    effacerMesure();
    void    annulerMesurePerdue(const QString& message = QString());
    bool    pointsMesureConfondus() const; // deux points posés au même endroit : distance nulle
    void    retirerSegmentMesure();
    void    retirerMarqueurAccroche();
    bool    pointMesure(gp_Pnt& point, bool& sommet) const;
    // Point d'accroche (sommet, milieu, centre...) ou point libre sous le curseur ; « libre » = pas d'accroche.
    bool    chercherAccroche(const QPoint& ecran, ancrage::Ancrage& choix, bool& libre) const;
    void    ajouterPointMesure(const QPoint& ecran);
    void    mettreAJourApercuMesure(const QPoint& ecran);
    void    dessinerSegmentMesure(const gp_Pnt& a, const gp_Pnt& b);
    void    redessinerMesure();
    void    afficherMarqueur(Handle(AIS_Point)& marqueur, const gp_Pnt& p, const Quantity_Color& couleur);
    // La mesure suit la géométrie : après une modification du document, ou pendant un geste.
    void    actualiserMesure(const Document& document);
    void    memoriserPositionsMesure();
    void    suivreMesureRigide();
    void    suivreMesureEtirement(const TopoDS_Shape& nouvelle, double distance, bool impossible);
    QString texteMesure(const gp_Pnt& a, const gp_Pnt& b, bool detail) const;
    static QString nomAccroche(ancrage::Genre genre);

    Handle(V3d_Viewer)             m_viewer;
    Handle(V3d_View)               m_vue;
    Handle(AIS_InteractiveContext) m_contexte;
    Handle(GrilleAxes)             m_grille;
    Handle(AIS_Shape)              m_plateau;
    Handle(AIS_Shape)              m_volumePlateau;
    std::vector<ObjetAffiche>      m_objets;
    ModeSelection                  m_mode = ModeSelection::Piece;
    QPoint                         m_dernierePosition;
    QPoint                         m_positionPression;
    bool                           m_premierAffichage = true;

    Geste  m_geste = Geste::Aucun;
    bool   m_gesteDemarre = false;
    double m_pasAimantation = 1.0;

    // Déplacement
    std::vector<Handle(AIS_Shape)> m_objetsDeplaces;
    QVector<int>                   m_idsDeplaces;
    gp_Pnt                         m_pointDepart;
    double                         m_coinX = 0.0;
    double                         m_coinY = 0.0;
    double                         m_dx = 0.0;
    double                         m_dy = 0.0;
    std::vector<gp_Pnt>            m_centresDeplaces;

    // Rotation à la souris
    bool        m_outilRotation = false;
    AxeRotation m_axe = AxeRotation::Z;
    double      m_pasAngle = 15.0;
    double      m_angle = 0.0;

    // Étirement
    Handle(AIS_Shape) m_objetEtire;
    TopoDS_Shape      m_formeInitiale;
    TopoDS_Shape      m_faceEtiree;
    int               m_idEtire = 0;
    gp_Pnt            m_pointFace;
    gp_Vec            m_normaleFace;
    double            m_s0 = 0.0;
    double            m_distance = 0.0;

    // Esquisse
    bool                      m_esquisseActive = false;
    gp_Ax3                    m_repere;
    OutilEsquisse             m_outilEsquisse = OutilEsquisse::Rectangle;
    std::vector<gp_Pnt2d>     m_pointsEnCours;
    std::vector<TopoDS_Shape> m_profils;
    std::vector<Profil>       m_descriptions; // même ordre que m_profils
    Handle(AIS_Shape)         m_objetProfils;
    Handle(CotesEsquisse)     m_objetCotes;
    std::vector<InfoCote>     m_infosCotes;
    Handle(SelectMgr_EntityOwner) m_dernierSurvol; // dernier élément survolé (pour limiter les redessins)
    bool                      m_ignorerRelache = false;
    Handle(AIS_Shape)         m_objetApercu;
    QString                   m_saisie;     // cotes tapées au clavier
    gp_Pnt2d                  m_dernierUV;  // dernière position de la souris dans le plan
    QLabel*                   m_bulle = nullptr;
    QElapsedTimer             m_horlogeSurvol;            // limite la cadence du survol
    QTimer*                   m_minuterieSurvol = nullptr; // applique le dernier survol en attente
    QPoint                    m_survolEnAttente;

    // Mesure de distance
    bool                                    m_mesureActive = false;
    // Pendant un étirement : un point mesuré a (provisoirement) disparu, la mesure est masquée ;
    // si c'est encore le cas au relâchement, la mesure est annulée.
    bool                                    m_mesurePerdueGeste = false;
    std::vector<ancrage::Ancrage>           m_ancragesMesure;    // 0, 1 ou 2 points mesurés
    std::map<int, TopoDS_Shape>             m_formesMesure;      // forme des pièces mesurées, à la dernière mise à jour
    std::vector<gp_Pnt>                     m_positionsAvantGeste;
    std::vector<ancrage::Ancrage>           m_ancragesAvantGeste; // ancrages au début du geste (étirement)
    std::map<int, gp_Pnt>                   m_centresPieces;     // cache des centres de gravité
    Handle(AIS_Point)                       m_marqueurAccroche;  // repère vert du point d'accroche visé
    std::vector<Handle(AIS_Point)>          m_marqueursMesure;
    Handle(AIS_Shape)                       m_ligneMesure;
    Handle(AIS_TextLabel)                   m_texteMesure;

    Handle(AIS_Shape)                       m_paroisFines;
};
