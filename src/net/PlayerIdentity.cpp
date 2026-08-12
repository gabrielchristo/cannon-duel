#include "PlayerIdentity.h"
#include "../Platform.h"


#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <random>

std::string PlayerIdentity::SavePath() {
    // Fica ao lado do executável, junto com o resto do jogo — simples e
    // suficiente pra um projeto hobby (sem depender de diretórios de config
    // do sistema operacional).
    return "player_identity.txt";
}

std::string PlayerIdentity::GeneratePseudoUuid() {
    // Não precisa ser criptograficamente forte — é só um identificador
    // local pra distinguir jogadores no lobby, não um token de segurança.
    static std::random_device rd;
    static std::mt19937_64 rng(rd() ^ static_cast<unsigned long long>(time(nullptr)));
    std::uniform_int_distribution<int> hexDist(0, 15);

    const char* hexChars = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (char& c : uuid) {
        if (c == 'x') {
            c = hexChars[hexDist(rng)];
        } else if (c == 'y') {
            c = hexChars[8 + (hexDist(rng) % 4)]; // variante UUID v4 (8,9,a,b)
        }
    }
    return uuid;
}

void PlayerIdentity::LoadOrCreate() {
    std::ifstream in(SavePath());
    if (in.good()) {
        std::string line1, line2;
        std::getline(in, line1);
        std::getline(in, line2);
        if (!line1.empty()) {
            id = line1;
            displayName = line2.empty() ? ("Canhoneiro#" + id.substr(0, 4)) : line2;
            return;
        }
    }

    // Primeira vez rodando — gera um novo identificador e um nome padrão.
    id = GeneratePseudoUuid();
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    int suffix = 1000 + static_cast<int>(rng() % 9000);
    displayName = "Canhoneiro#" + std::to_string(suffix);
    Save();
}

void PlayerIdentity::SetDisplayName(const std::string& name) {
    if (name.empty()) return;
    displayName = name;
    Save();
}

void PlayerIdentity::Save() const {
    std::ofstream out(SavePath());
    out << id << "\n" << displayName << "\n";
}

