#pragma once
#include <string>

// Identificador leve local do jogador — NÃO é uma conta/login. Gerado uma
// única vez, persistido em disco, e reutilizado em toda sessão futura.
// Serve só pra: (1) aparecer como um "nome" no lobby público, e (2) manter
// o histórico de vitórias/derrotas online associado a essa instalação do
// jogo, sem exigir e-mail, senha, ou qualquer verificação externa.
class PlayerIdentity {
public:
    // Carrega do disco se já existir; senão, gera um novo id + nome padrão
    // e salva. Chame uma vez, no início do programa.
    void LoadOrCreate();

    const std::string& Id() const { return id; }
    const std::string& DisplayName() const { return displayName; }

    // Permite o jogador personalizar o nome exibido no lobby (opcional).
    void SetDisplayName(const std::string& name);

private:
    std::string id;
    std::string displayName;

    static std::string SavePath();
    void Save() const;
    static std::string GeneratePseudoUuid();
};
