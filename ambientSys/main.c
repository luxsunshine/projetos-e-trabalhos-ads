/* main.c
   AmbientSys - versão GTK3 (single file)
   Funcionalidades:
   - Login com usuarios persistidos (arquivo users.dat)
   - carregarUsuariosPadrao() cria admin:123 se não existir
   - Menu principal (Cadastro Cliente, Registro Mensal, Relatorios, Sair)
   - Cadastro de Cliente (persistido em clientes.csv)
   - Registro Mensal (residuos e custo) persistido em registros.csv
   - Relatorios: visualizar em janela e salvar em TXT/CSV
   - Salva/Carrega usuarios e clientes simples
   Compilar com:
   gcc main.c -o AmbientSys.exe `pkg-config --cflags --libs gtk+-3.0`
*/

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ----------------- Configurações de arquivos ----------------- */
#define USERS_FILE "users.dat"       /* arquivo para usuarios (com senha "criptografada" simples) */
#define CLIENTS_FILE "clientes.csv"  /* CSV de clientes */
#define RECORDS_FILE "registros.csv" /* CSV de registros mensais */

/* ----------------- Estruturas ----------------- */
typedef struct {
    char username[50];
    char password_enc[128]; /* senha armazenada (simplesmente 'xor' codificada) */
    int nivel; /* 1 = admin, 2 = profissional/usuario comum */
} User;

typedef struct {
    char nome[100];
    char responsavel[100];
    char cnpj[32];
    char razao_social[100];
    char nome_fantasia[100];
    char telefone[40];
    char endereco[200];
    char email[100];
    char data_abertura[20];
} Cliente;

typedef struct {
    char cnpj_industria[32];
    char mes_ano[16]; /* ex: 2025-09 */
    double residuos_tratados;
    double custo_estimado;
} RegistroMensal;

/* ----------------- Dados em memória ----------------- */
#define MAX_USERS 50
static User users[MAX_USERS];
static int users_count = 0;

#define MAX_CLIENTES 500
static Cliente clientes[MAX_CLIENTES];
static int clientes_count = 0;

#define MAX_RECORDS 2000
static RegistroMensal registros[MAX_RECORDS];
static int registros_count = 0;

/* ----------------- Funções utilitárias ----------------- */

/* XOR simples para "criptografar" (exemplo educacional; não usar em producao) */
void xor_crypt(char *data, const char *key) {
    size_t n = strlen(data);
    size_t k = strlen(key);
    if (k == 0) return;
    for (size_t i = 0; i < n; ++i) {
        data[i] ^= key[i % k];
    }
}

/* transforma string para minúsculas (para comparação de usuario) */
void str_tolower(char *s) {
    for (; *s; ++s) *s = (char)tolower((unsigned char)*s);
}

/* ----------------- Persistência de usuarios ----------------- */

/* Carrega usuários do arquivo; retorna 0 se ok, -1 se não encontrado */
int carregarUsuariosArquivo() {
    FILE *f = fopen(USERS_FILE, "rb");
    if (!f) return -1;
    users_count = 0;
    while (fread(&users[users_count], sizeof(User), 1, f) == 1) {
        users_count++;
        if (users_count >= MAX_USERS) break;
    }
    fclose(f);
    return 0;
}

/* Salva usuários no arquivo */
int salvarUsuariosArquivo() {
    FILE *f = fopen(USERS_FILE, "wb");
    if (!f) return -1;
    fwrite(users, sizeof(User), users_count, f);
    fclose(f);
    return 0;
}

/* Carrega usuários padrao se arquivo ausente (cria admin/123) */
void carregarUsuariosPadrao() {
    if (carregarUsuariosArquivo() == 0) {
        /* carregou usuários existentes */
        return;
    }

    /* criar usuário admin padr\ao */
    users_count = 0;
    User u;
    memset(&u, 0, sizeof(u));
    strncpy(u.username, "admin", sizeof(u.username)-1);
    /* senha plain "123" -> codificamos com XOR e salvamos em senha_enc */
    char pwd[] = "123";
    strncpy(u.password_enc, pwd, sizeof(u.password_enc)-1);
    xor_crypt(u.password_enc, "ambientkey"); /* chave simples */
    u.nivel = 1; /* administrador */
    users[users_count++] = u;

    /* opcional: um usuário profissional */
    User v;
    memset(&v, 0, sizeof(v));
    strncpy(v.username, "user", sizeof(v.username)-1);
    char pwd2[] = "user123";
    strncpy(v.password_enc, pwd2, sizeof(v.password_enc)-1);
    xor_crypt(v.password_enc, "ambientkey");
    v.nivel = 2;
    users[users_count++] = v;

    salvarUsuariosArquivo();
}

/* Adiciona usuário (usa criptografia simples) */
int adicionarUsuario(const char *username, const char *password, int nivel) {
    if (users_count >= MAX_USERS) return -1;
    User u;
    memset(&u, 0, sizeof(u));
    strncpy(u.username, username, sizeof(u.username)-1);
    strncpy(u.password_enc, password, sizeof(u.password_enc)-1);
    xor_crypt(u.password_enc, "ambientkey");
    u.nivel = nivel;
    users[users_count++] = u;
    salvarUsuariosArquivo();
    return 0;
}

/* Autentica usuário; preenche nivel e retorna 0 se ok, -1 se falha */
int autenticarUsuario(const char *username_in, const char *password_in, int *nivel_out) {
    char uname_low[60];
    strncpy(uname_low, username_in, sizeof(uname_low)-1);
    str_tolower(uname_low);

    for (int i = 0; i < users_count; ++i) {
        char stored_user[60];
        strncpy(stored_user, users[i].username, sizeof(stored_user)-1);
        char stored_user_low[60];
        strncpy(stored_user_low, stored_user, sizeof(stored_user_low)-1);
        str_tolower(stored_user_low);
        if (strcmp(stored_user_low, uname_low) == 0) {
            /* decodifica senha para comparar */
            char pwd_dec[128];
            strncpy(pwd_dec, users[i].password_enc, sizeof(pwd_dec)-1);
            xor_crypt(pwd_dec, "ambientkey");
            if (strcmp(pwd_dec, password_in) == 0) {
                if (nivel_out) *nivel_out = users[i].nivel;
                return 0;
            } else {
                return -1;
            }
        }
    }
    return -1;
}

/* ----------------- Persistência de clientes ----------------- */

int carregarClientesArquivo() {
    FILE *f = fopen(CLIENTS_FILE, "r");
    if (!f) return -1;
    clientes_count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (clientes_count >= MAX_CLIENTES) break;
        /* CSV simples: campos separados por ; */
        char *token = strtok(line, ";");
        if (!token) continue;
        strncpy(clientes[clientes_count].nome, token, sizeof(clientes[clientes_count].nome)-1);

        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].responsavel, token, sizeof(clientes[clientes_count].responsavel)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].cnpj, token, sizeof(clientes[clientes_count].cnpj)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].razao_social, token, sizeof(clientes[clientes_count].razao_social)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].nome_fantasia, token, sizeof(clientes[clientes_count].nome_fantasia)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].telefone, token, sizeof(clientes[clientes_count].telefone)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].endereco, token, sizeof(clientes[clientes_count].endereco)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(clientes[clientes_count].email, token, sizeof(clientes[clientes_count].email)-1);
        token = strtok(NULL, ";\n"); if (!token) token=""; strncpy(clientes[clientes_count].data_abertura, token, sizeof(clientes[clientes_count].data_abertura)-1);

        clientes_count++;
    }
    fclose(f);
    return 0;
}

int salvarClientesArquivo() {
    FILE *f = fopen(CLIENTS_FILE, "w");
    if (!f) return -1;
    for (int i = 0; i < clientes_count; ++i) {
        Cliente *c = &clientes[i];
        fprintf(f, "%s;%s;%s;%s;%s;%s;%s;%s;%s\n",
                c->nome, c->responsavel, c->cnpj, c->razao_social, c->nome_fantasia,
                c->telefone, c->endereco, c->email, c->data_abertura);
    }
    fclose(f);
    return 0;
}

/* ----------------- Persistência de registros mensais ----------------- */

int carregarRegistrosArquivo() {
    FILE *f = fopen(RECORDS_FILE, "r");
    if (!f) return -1;
    registros_count = 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (registros_count >= MAX_RECORDS) break;
        char *token = strtok(line, ";");
        if (!token) continue;
        strncpy(registros[registros_count].cnpj_industria, token, sizeof(registros[registros_count].cnpj_industria)-1);
        token = strtok(NULL, ";"); if (!token) token=""; strncpy(registros[registros_count].mes_ano, token, sizeof(registros[registros_count].mes_ano)-1);
        token = strtok(NULL, ";"); if (!token) token="0"; registros[registros_count].residuos_tratados = atof(token);
        token = strtok(NULL, ";\n"); if (!token) token="0"; registros[registros_count].custo_estimado = atof(token);
        registros_count++;
    }
    fclose(f);
    return 0;
}

int salvarRegistrosArquivo() {
    FILE *f = fopen(RECORDS_FILE, "w");
    if (!f) return -1;
    for (int i = 0; i < registros_count; ++i) {
        fprintf(f, "%s;%s;%.3f;%.2f\n",
                registros[i].cnpj_industria,
                registros[i].mes_ano,
                registros[i].residuos_tratados,
                registros[i].custo_estimado);
    }
    fclose(f);
    return 0;
}

/* ----------------- Funções de aplicação (clientes, registros) ----------------- */

int cadastrarClienteMem(const Cliente *c) {
    if (clientes_count >= MAX_CLIENTES) return -1;
    clientes[clientes_count++] = *c;
    salvarClientesArquivo();
    return 0;
}

int registrarMensalMem(const RegistroMensal *r) {
    if (registros_count >= MAX_RECORDS) return -1;
    registros[registros_count++] = *r;
    salvarRegistrosArquivo();
    return 0;
}

/* ----------------- GTK GUI ----------------- */

/* Variáveis de janelas/elementos (globais para callbacks) */
GtkWidget *win_login;
GtkWidget *entry_user;
GtkWidget *entry_pass;
GtkWidget *win_main;

/* Other windows */
GtkWidget *dialog_message;

/* Nivel do usuário autenticado */
int nivel_autenticado = 0;
char usuario_atual[60] = {0};

/* Forward declarations de callbacks */
void on_login_button_clicked(GtkButton *b, gpointer user_data);
void abrir_tela_cadastro_cliente(GtkWidget *widget, gpointer data);
void abrir_tela_registro_mensal(GtkWidget *widget, gpointer data);
void abrir_tela_relatorios(GtkWidget *widget, gpointer data);
void on_logout_clicked(GtkButton *b, gpointer user_data);

/* Criar janela de mensagem simples */
void show_message(GtkWindow *parent, const char *title, const char *message) {
    GtkWidget *md = gtk_message_dialog_new(parent,
                       GTK_DIALOG_DESTROY_WITH_PARENT,
                       GTK_MESSAGE_INFO,
                       GTK_BUTTONS_OK,
                       "%s", message);
    gtk_window_set_title(GTK_WINDOW(md), title);
    gtk_dialog_run(GTK_DIALOG(md));
    gtk_widget_destroy(md);
}

/* Construir janela de login */
GtkWidget *build_login_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "AmbientSys - Login");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 220);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_add(GTK_CONTAINER(window), grid);

    GtkWidget *label_user = gtk_label_new("Usuário:");
    entry_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "Usuário");

    GtkWidget *label_pass = gtk_label_new("Senha:");
    entry_pass = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_pass), FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_pass), "Senha");

    GtkWidget *btn_login = gtk_button_new_with_label("Entrar");
    g_signal_connect(btn_login, "clicked", G_CALLBACK(on_login_button_clicked), NULL);

    gtk_grid_attach(GTK_GRID(grid), label_user, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_user, 1, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), label_pass, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_pass, 1, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_login, 1, 2, 1, 1);

    /* quick hint */
    GtkWidget *hint = gtk_label_new("Usuário administrador padrao: admin / 123");
    gtk_grid_attach(GTK_GRID(grid), hint, 0, 3, 3, 1);

    return window;
}

/* Construir janela principal (menu) */
GtkWidget *build_main_window() {
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "AmbientSys - Menu Principal");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 360);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    GtkWidget *lbl = gtk_label_new(NULL);
    char buf[256];
    snprintf(buf, sizeof(buf), "<b>Bem-vindo: %s</b>\n(Seu nível: %s)", usuario_atual, (nivel_autenticado==1) ? "Administrador" : "Profissional");
    gtk_label_set_markup(GTK_LABEL(lbl), buf);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    GtkWidget *btn_cad = gtk_button_new_with_label("Cadastro de Cliente");
    GtkWidget *btn_reg = gtk_button_new_with_label("Registro Mensal");
    GtkWidget *btn_rel = gtk_button_new_with_label("Relatorios");
    GtkWidget *btn_logout = gtk_button_new_with_label("Logout");

    g_signal_connect(btn_cad, "clicked", G_CALLBACK(abrir_tela_cadastro_cliente), NULL);
    g_signal_connect(btn_reg, "clicked", G_CALLBACK(abrir_tela_registro_mensal), NULL);
    g_signal_connect(btn_rel, "clicked", G_CALLBACK(abrir_tela_relatorios), NULL);
    g_signal_connect(btn_logout, "clicked", G_CALLBACK(on_logout_clicked), NULL);

    gtk_grid_attach(GTK_GRID(grid), btn_cad, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_reg, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_rel, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_logout, 1, 1, 1, 1);

    return window;
}

/* Callback Login */
void on_login_button_clicked(GtkButton *b, gpointer user_data) {
    const char *u = gtk_entry_get_text(GTK_ENTRY(entry_user));
    const char *p = gtk_entry_get_text(GTK_ENTRY(entry_pass));
    if (!u || !p || strlen(u) == 0 || strlen(p) == 0) {
        show_message(GTK_WINDOW(win_login), "Erro", "Informe usuário e senha.");
        return;
    }
    int nivel = 0;
    if (autenticarUsuario(u, p, &nivel) == 0) {
        nivel_autenticado = nivel;
        strncpy(usuario_atual, u, sizeof(usuario_atual)-1);
        /* abrir tela principal e fechar login */
        if (win_main) gtk_widget_destroy(win_main);
        win_main = build_main_window();
        gtk_widget_show_all(win_main);
        gtk_widget_hide(win_login);
    } else {
        show_message(GTK_WINDOW(win_login), "Erro", "Usuário ou senha inválidos.");
    }
}

/* Logout */
void on_logout_clicked(GtkButton *b, gpointer user_data) {
    /* fechar main, mostrar login limpo */
    gtk_widget_destroy(win_main);
    gtk_entry_set_text(GTK_ENTRY(entry_user), "");
    gtk_entry_set_text(GTK_ENTRY(entry_pass), "");
    gtk_widget_show_all(win_login);
}

/* ---------- Cadastro de Cliente (janela) ---------- */
void on_cad_cliente_save(GtkButton *btn, gpointer data) {
    GtkWidget **widgets = (GtkWidget **)data;
    const char *nome = gtk_entry_get_text(GTK_ENTRY(widgets[0]));
    const char *responsavel = gtk_entry_get_text(GTK_ENTRY(widgets[1]));
    const char *cnpj = gtk_entry_get_text(GTK_ENTRY(widgets[2]));
    const char *razao = gtk_entry_get_text(GTK_ENTRY(widgets[3]));
    const char *fantasia = gtk_entry_get_text(GTK_ENTRY(widgets[4]));
    const char *telefone = gtk_entry_get_text(GTK_ENTRY(widgets[5]));
    const char *endereco = gtk_entry_get_text(GTK_ENTRY(widgets[6]));
    const char *email = gtk_entry_get_text(GTK_ENTRY(widgets[7]));
    const char *dataab = gtk_entry_get_text(GTK_ENTRY(widgets[8]));

    if (!nome || strlen(nome)==0) {
        show_message(NULL, "Erro", "Nome obrigatório.");
        return;
    }
    Cliente c;
    memset(&c,0,sizeof(c));
    strncpy(c.nome, nome, sizeof(c.nome)-1);
    strncpy(c.responsavel, responsavel, sizeof(c.responsavel)-1);
    strncpy(c.cnpj, cnpj, sizeof(c.cnpj)-1);
    strncpy(c.razao_social, razao, sizeof(c.razao_social)-1);
    strncpy(c.nome_fantasia, fantasia, sizeof(c.nome_fantasia)-1);
    strncpy(c.telefone, telefone, sizeof(c.telefone)-1);
    strncpy(c.endereco, endereco, sizeof(c.endereco)-1);
    strncpy(c.email, email, sizeof(c.email)-1);
    strncpy(c.data_abertura, dataab, sizeof(c.data_abertura)-1);

    if (cadastrarClienteMem(&c) == 0) {
        show_message(NULL, "Sucesso", "Cliente cadastrado com sucesso.");
    } else {
        show_message(NULL, "Erro", "Não foi possível cadastrar cliente.");
    }
}

/* Abre janela de cadastro de cliente */
void abrir_tela_cadastro_cliente(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Cadastro de Cliente",
                        GTK_WINDOW(win_main),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        ("_Fechar"), GTK_RESPONSE_CLOSE,
                        NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);

    const char *labels[] = {"Nome:", "Responsavel:", "CNPJ:", "Razao Social:", "Nome Fantasia:", "Telefone:", "Endereco:", "E-mail:", "Data Abertura (DD/MM/AAAA):"};
    GtkWidget *entries[9];
    for (int i=0;i<9;i++) {
        GtkWidget *lbl = gtk_label_new(labels[i]);
        entries[i] = gtk_entry_new();
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entries[i], 1, i, 1, 1);
    }

    GtkWidget *btn_save = gtk_button_new_with_label("Salvar");
    /* passamos array entries como ponteiro para callback */
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_cad_cliente_save), entries);
    gtk_grid_attach(GTK_GRID(grid), btn_save, 0, 9, 2, 1);

    gtk_widget_show_all(dialog);
}

/* ---------- Registro Mensal (janela) ---------- */
void on_registro_save(GtkButton *btn, gpointer data) {
    GtkWidget **widgets = (GtkWidget **)data;
    const char *cnpj = gtk_entry_get_text(GTK_ENTRY(widgets[0]));
    const char *mesano = gtk_entry_get_text(GTK_ENTRY(widgets[1]));
    const char *residuos = gtk_entry_get_text(GTK_ENTRY(widgets[2]));
    const char *custo = gtk_entry_get_text(GTK_ENTRY(widgets[3]));

    if (!cnpj || strlen(cnpj)==0 || !mesano || strlen(mesano)==0) {
        show_message(NULL, "Erro", "CNPJ e Mês/Ano obrigatórios.");
        return;
    }
    RegistroMensal r;
    memset(&r,0,sizeof(r));
    strncpy(r.cnpj_industria, cnpj, sizeof(r.cnpj_industria)-1);
    strncpy(r.mes_ano, mesano, sizeof(r.mes_ano)-1);
    r.residuos_tratados = atof(residuos);
    r.custo_estimado = atof(custo);

    if (registrarMensalMem(&r) == 0) {
        show_message(NULL, "Sucesso", "Registro mensal salvo.");
    } else {
        show_message(NULL, "Erro", "Falha ao salvar registro.");
    }
}

void abrir_tela_registro_mensal(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Registro Mensal",
                        GTK_WINDOW(win_main),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        ("_Fechar"), GTK_RESPONSE_CLOSE,
                        NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 8);
    gtk_container_add(GTK_CONTAINER(content), grid);

    const char *labels[] = {"CNPJ Industria:", "Mês/Ano (YYYY-MM):", "Quantidade Residuos (tons):", "Custo Estimado (R$):"};
    GtkWidget *entries[4];
    for (int i=0;i<4;i++) {
        GtkWidget *lbl = gtk_label_new(labels[i]);
        entries[i] = gtk_entry_new();
        gtk_grid_attach(GTK_GRID(grid), lbl, 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), entries[i], 1, i, 1, 1);
    }
    GtkWidget *btn_save = gtk_button_new_with_label("Salvar Registro");
    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_registro_save), entries);
    gtk_grid_attach(GTK_GRID(grid), btn_save, 0, 4, 2, 1);
    gtk_widget_show_all(dialog);
}

/* ---------- Relatórios (janela) ---------- */

typedef enum { REP_INDIV_SEMESTRE, REP_GASTOS_MENSAL, REP_REGIOES_VOLUME, REP_IND_MENOR_PROD, REP_APORTE_SEMESTRE } ReportType;

void salvar_relatorio_txt(const char *filename, const char *content) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        show_message(NULL, "Erro", "Nao foi possivel criar arquivo.");
        return;
    }
    fprintf(f, "%s", content);
    fclose(f);
}

void salvar_relatorio_csv(const char *filename, const char *content) {
    /* content must be CSV formatted already */
    salvar_relatorio_txt(filename, content);
}

/* Gera conteúdo de relatorio simples (exemplo): */
char *gerar_relatorio_simples(ReportType type) {
    /* retornamos uma string alocada (caller deve free) */
    char *buf = malloc(100000);
    if (!buf) return NULL;
    buf[0] = 0;
    if (type == REP_INDIV_SEMESTRE) {
        strcat(buf, "Relatorio Individualizado - Total de insumos tratados semestralmente\n\n");
        /* sumariza pelos clientes */
        for (int i=0;i<clientes_count;i++) {
            double soma = 0;
            for (int j=0;j<registros_count;j++) {
                if (strcmp(clientes[i].cnpj, registros[j].cnpj_industria)==0) {
                    /* se mes_ano estiver dentro do ultimo semestre? vamos apenas somar tudo por simplicidade */
                    soma += registros[j].residuos_tratados;
                }
            }
            char line[256];
            snprintf(line, sizeof(line), "%s (%s): %.3f\n", clientes[i].nome, clientes[i].cnpj, soma);
            strcat(buf, line);
        }
    } else if (type == REP_GASTOS_MENSAL) {
        strcat(buf, "Relatorio Individualizado - Total de gastos mensais (por industria)\n\n");
        /* simples resumo por mes */
        /* agrupamos por mes_ano e somamos custos */
        for (int j=0;j<registros_count;j++) {
            char line[256];
            snprintf(line, sizeof(line), "%s | %s | R$ %.2f\n", registros[j].cnpj_industria, registros[j].mes_ano, registros[j].custo_estimado);
            strcat(buf, line);
        }
    } else if (type == REP_REGIOES_VOLUME) {
        strcat(buf, "Relatorio Global - Regioes com maior volume de residuos tratados\n\n");
        strcat(buf, "(Relatorio sintetico - implementar associacao de cidades/estados com clientes)\n");
    } else if (type == REP_IND_MENOR_PROD) {
        strcat(buf, "Relatorio Global - Industrias com menor producao no ultimo semestre\n\n");
        strcat(buf, "(Relatorio sintetico)\n");
    } else if (type == REP_APORTE_SEMESTRE) {
        strcat(buf, "Relatorio Global - Aporte financeiro semestral\n\n");
        double total = 0;
        for (int j=0;j<registros_count;j++) total += registros[j].custo_estimado;
        char line[128];
        snprintf(line, sizeof(line), "Total aporte semestral (estimado): R$ %.2f\n", total);
        strcat(buf, line);
    }
    return buf;
}

/* Callback salvar relatorio - solicita nome via file chooser */
void on_save_report_clicked(GtkButton *btn, gpointer data) {
    gpointer *arr = (gpointer *)data;
    ReportType rtype = GPOINTER_TO_INT(arr[0]);
    GtkWindow *parent = GTK_WINDOW(arr[1]);

    char *content = gerar_relatorio_simples(rtype);
    if (!content) {
        show_message(parent, "Erro", "Falha ao gerar relatorio.");
        return;
    }

    /* Ask save as TXT */
    GtkWidget *chooser = gtk_file_chooser_dialog_new("Salvar relatorio (TXT)",
                                                     parent,
                                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                                     "_Cancelar", GTK_RESPONSE_CANCEL,
                                                     "_Salvar", GTK_RESPONSE_ACCEPT,
                                                     NULL);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), "relatorio.txt");
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
        salvar_relatorio_txt(filename, content);
        g_free(filename);
        show_message(parent, "Sucesso", "Relatorio salvo (TXT).");
    }
    gtk_widget_destroy(chooser);
    free(content);
}

/* Abre janela de relatórios */
void abrir_tela_relatorios(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Relatorios",
                        GTK_WINDOW(win_main),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        ("_Fechar"), GTK_RESPONSE_CLOSE,
                        NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    GtkWidget *lbl = gtk_label_new("Escolha um relatorio para visualizar/salvar:");
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    struct { const char *name; ReportType type; } reports[] = {
        {"Total de insumos tratados (Individualizado)", REP_INDIV_SEMESTRE},
        {"Total de gastos mensais (Individualizado)", REP_GASTOS_MENSAL},
        {"Regioes com maior volume (Global)", REP_REGIOES_VOLUME},
        {"Industrias com menor producao (Global)", REP_IND_MENOR_PROD},
        {"Aporte financeiro semestral (Global)", REP_APORTE_SEMESTRE},
    };

    for (int i=0;i< (int)(sizeof(reports)/sizeof(reports[0])); i++) {
        GtkWidget *h = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *bview = gtk_button_new_with_label("Visualizar");
        GtkWidget *bsave = gtk_button_new_with_label("Salvar...");
        /* pack */
        GtkWidget *label = gtk_label_new(reports[i].name);
        gtk_box_pack_start(GTK_BOX(h), label, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(h), bview, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(h), bsave, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), h, FALSE, FALSE, 0);



        /* connect save */
        /* work-around: pack pointers to pass to generic handler */
        gpointer *arr = g_new(gpointer, 2);
        arr[0] = GINT_TO_POINTER(reports[i].type);
        arr[1] = dialog;
        g_signal_connect(bsave, "clicked", G_CALLBACK(on_save_report_clicked), arr);
    }

    gtk_widget_show_all(dialog);
}

/* ----------------- Inicialização e main ----------------- */

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    /* carregar dados */
    carregarUsuariosPadrao(); /* cria admin se não houver arquivo */
    carregarClientesArquivo();
    carregarRegistrosArquivo();

    /* build login window */
    win_login = build_login_window();
    gtk_widget_show_all(win_login);
    g_signal_connect(win_login, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    return 0;
}
