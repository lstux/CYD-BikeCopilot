#pragma once
// Page « Trajets » : entrée tactile (la partie affichage et les actions sont dans common/src/histpage.h).

// Glissé vertical dans la liste (une ligne tous les ~22 px)
static void histDrag(int dy) {
  if (histSel >= 0 || histN <= pg_trips::ROWS) return;
  histAcc += dy;
  int old = histTop;
  while (histAcc <= -22 && histTop < histN - pg_trips::ROWS) { histTop++; histAcc += 22; }
  while (histAcc >= 22 && histTop > 0) { histTop--; histAcc -= 22; }
  if (histTop == 0 && histAcc > 0) histAcc = 0;
  if (histTop >= histN - pg_trips::ROWS && histAcc < 0) histAcc = 0;
  if (histTop != old) drawHistRows();
}

// Curseur des courbes : x = position du doigt à l'écran
static void histCursorAt(int x) {
  using namespace pg_trips;
  if (histSel < 0 || !histSprOk || histConfirm) return;
  int c = x - 5 - CH_X0;
  const int W = CH_X1 - CH_X0 + 1;
  if (c < 0) c = 0;
  if (c > W - 1) c = W - 1;
  if (c == histCursor) return;
  histCursor = c;
  histDrawCharts();
}

// Retourne true si le toucher a été traité par la page
static bool histTap(int x, int y) {
  using namespace pg_trips;
  if (histSel >= 0) {
    if (histConfirm) {
      histAnswerDelete(y >= 118 && y <= 148 && x >= 160 && x <= 270);  // confirmer, sinon annuler
      return true;
    }
    if (y < 46) {
      if (x >= 236) {
        histAskDelete();
      } else if (x >= 166) {  // « Destination » : fin de ce trajet
        histSetDestFromTrip();
      } else {
        histBack();  // en-tête : retour à la liste
      }
    } else if (y >= CHART_Y[0]) {
      histCursorAt(x);
    }
    return true;
  }
  histAcc = 0;
  if (x >= 303) {  // barre de défilement : page précédente / suivante
    histScroll(y < LIST_Y + ROWS * ROW_H / 2 ? -ROWS : ROWS);
    return true;
  }
  int r = (y - LIST_Y) / ROW_H;
  if (y >= LIST_Y && r >= 0 && r < ROWS && histTop + r < histN) histOpen(histTop + r);
  return true;
}
