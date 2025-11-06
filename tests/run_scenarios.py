from common import (
    start_server, stop_server, mk_client, send_line, recv_until, recv_some, assert_contains
)
import time, sys

def scenario_disconnect_midgame():
    """
    Ines LOGIN, Malek LOGIN,
    Ines CHALLENGE Malek, Malek ACCEPT Ines,
    Ines se déconnecte → Malek reçoit GAME_ABORT + redevient disponible,
    LIST ne doit plus contenir Ines.
    """
    logs = []

    c1 = mk_client(); logs.append("c1 connected")
    c2 = mk_client(); logs.append("c2 connected")

    # LOGIN
    send_line(c1, "LOGIN Ines")
    out1 = recv_until(c1, "Connexion réussie", 1.0); logs.append(("c1", out1))
    assert_contains(out1, "Connexion réussie", out1)

    send_line(c2, "LOGIN Malek")
    out2 = recv_until(c2, "Connexion réussie", 1.0); logs.append(("c2", out2))
    assert_contains(out2, "Connexion réussie", out2)

    # CHALLENGE Ines -> Malek
    send_line(c1, "CHALLENGE Malek")
    # Malek reçoit la notif
    notif = recv_until(c2, "CHALLENGE_FROM Ines", 1.0); logs.append(("c2", notif))
    assert_contains(notif, "CHALLENGE_FROM Ines", notif)

    # ACCEPT côté Malek
    send_line(c2, "ACCEPT Ines")
    # Les deux reçoivent CHALLENGE_ACCEPTED + GAME_START
    gs1 = recv_until(c1, "GAME_START", 1.0) + recv_some(c1); logs.append(("c1", gs1))
    gs2 = recv_until(c2, "GAME_START", 1.0) + recv_some(c2); logs.append(("c2", gs2))
    assert_contains(gs1, "GAME_START", gs1)
    assert_contains(gs2, "GAME_START", gs2)

    # Ines se déconnecte brutalement (fermeture socket)
    c1.close(); logs.append("c1 closed")

    # Malek doit recevoir GAME_ABORT
    abort_msg = recv_until(c2, "GAME_ABORT", 2.0); logs.append(("c2", abort_msg))
    assert_contains(abort_msg, "GAME_ABORT", abort_msg)

    # Vérifie la liste : Ines ne doit plus apparaître
    send_line(c2, "LIST")
    lst = recv_until(c2, "\n", 1.0); logs.append(("c2", lst))
    if "Ines" in lst:
        raise AssertionError("Ines apparaît encore dans LIST (attendu: offline/absente).")

    # Optionnel: vérifier que Malek n’est plus occupé (difficile sans commande STATE).
    # On peut tenter un auto-challenge -> doit dire 'tu ne peux pas te défier toi-même' (ok)
    # ou un BROADCAST implicitement accepté. On s’arrête là pour le test minimal.
    c2.close()

    return True, logs

def main():
    start_server()
    try:
        ok, logs = scenario_disconnect_midgame()
        print("✅ Scenario 'disconnect_midgame' PASSED")
        sys.exit(0)
    except AssertionError as e:
        print("❌ Scenario FAILED:", e)
        sys.exit(1)
    finally:
        stop_server()

if __name__ == "__main__":
    main()
