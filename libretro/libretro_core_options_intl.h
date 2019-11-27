#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include <libretro.h>

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

struct retro_core_option_definition option_defs_pt_br[] = {
   {
      "genesis_plus_gx_system_hw",
      "Hardware do sistema",
      "Executa o conteúdo carregado com um console emulado específico. 'Automático' selecionará o sistema mais apropriado para o jogo atual.",
      {
         { "auto",                 NULL },
         { "sg-1000",              "SG-1000"            },
         { "sg-1000 II",           "SG-1000 II"         },
         { "mark-III",             "Mark III"           },
         { "master system",        "Master System"      },
         { "master system II",     "Master System II"   },
         { "game gear",            "Game Gear"          },
         { "mega drive / genesis", "Mega Drive" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_region_detect",
      "Região do sistema",
      "Especifica de que região o sistema é. Para consoles que não sejam o Game Gear, 'PAL' é de 50hz enquanto 'NTSC' é de 60hz. s jogos podem ser executados mais rapidamente ou mais lentamente do que o normal se a região incorreta estiver selecionada.",
      {
         { "auto",    NULL },
         { "ntsc-u",  "NTSC-U" },
         { "pal",     "PAL"    },
         { "ntsc-j",  "NTSC-J" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_force_dtack",
      "Bloqueios do sistema",
      "Emula os bloqueios do sistema que ocorrem no hardware real ao executar acesso ilegal a endereços. Isso só deve ser desativado ao reproduzir certas demos e homebrews que dependem de comportamento ilegal para a operação correta.",
      {
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_bios",
      "ROM de inicialização do sistema",
      "Usa a BIOS/bootloader oficial para o hardware emulado, se estiver presente no diretório de sistema do RetroArch. Exibe a sequência/animação da inicialização específica do console e executa o conteúdo carregado.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_bram",
      "Sistema BRAM do CD",
      "Ao executar o conteúdo de CD da Sega, especifica se um único arquivo salvo será salvo entre todos os jogos de uma região específica (Por-BIOS) ou se será criado um arquivo salvo separado para cada jogo (Por-Jogo). Observe que o CD da Sega possui armazenamento interno limitado, suficiente apenas para alguns títulos. Para evitar ficar sem espaço, a configuração 'Por jogo' é recomendada.",
      {
         { "per bios", "Por-BIOS" },
         { "per game", "Por-Jogo" },
         { NULL, NULL },
      },
      "per bios"
   },
   {
      "genesis_plus_gx_addr_error",
      "Erro de endereçamento do 68K",
      "A CPU do Mega Drive (Motorola 68000) produz um erro de endereçamento (falha) ao tentar executar o acesso desalinhado à memória. A ativação do 'Erro de endereçamento do 68K' simula esse comportamento. Ele deve ser desativado apenas durante a execução de hacks de ROMs, pois geralmente são desenvolvidos usando emuladores menos precisos e podem contar com acesso inválido à RAM para a operação correta.",
      {
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_lock_on",
      "Cartucho Lock-On",
      "A tecnologia Lock-On é um recurso do Mega Drive que permite que um jogo mais antigo se conecte à porta de passagem de um cartucho especial para uma jogabilidade estendida ou alterada. Esta opção especifica qual tipo de cartucho especial 'lock-on' a ser emulado. Um arquivo do BIOS correspondente deve estar presente no diretório do sistema do RetroArch.",
      {
         { "disabled",            NULL },
         { "game genie",          "Game Genie" },
         { "action replay (pro)", "Action Replay (Pro)" },
         { "sonic & knuckles",    "Sonic & Knuckles" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_ym2413",
      "Som FM do Master System (YM2413)",
      "Ativa a emulação da unidade de som FM usada por certos jogos do Master System para obter uma saída de áudio aprimorada.",
      {
         { "auto",     NULL },
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_ym2612",
      "Som FM do Mega Drive",
#ifdef HAVE_YM3438_CORE
      "Seleciona o método usado para emular o sintetizador FM (gerador de som principal) do Mega Drive. As opções 'MAME' são rápidas e executam a toda velocidade na maioria dos sistemas. As opções 'Nuked' têm precisão de ciclo, qualidade muito alta e requisitos substanciais de CPU. O chip 'YM2612' é usado pelo Mega Drive Modelo 1 original. O 'YM3438' é usado em revisões posteriores do Mega Drive.",
#else
      "Seleciona o método usado para emular o sintetizador de FM (gerador de som principal) do Mega Drive. O chip 'YM2612' é usado pelo Mega Drive Modelo 1 original. O 'YM3438' é usado em revisões posteriores do Mega Drive.",
#endif
      {
         { "mame (ym2612)",          "MAME (YM2612)" },
         { "mame (asic ym3438)",     "MAME (ASIC YM3438)" },
         { "mame (enhanced ym3438)", "MAME (YM3438 aprimorado)" },
#ifdef HAVE_YM3438_CORE
         { "nuked (ym2612)",         "Nuked (YM2612)" },
         { "nuked (ym3438)",         "Nuked (YM3438)" },
#endif
         { NULL, NULL },
      },
      "mame (ym2612)"
   },
   {
      "genesis_plus_gx_sound_output",
      "Saída de som",
      "Selecione reprodução de som estéreo ou mono.",
      {
         { "stereo", "Estéreo" },
         { "mono",   "Mono" },
         { NULL, NULL },
      },
      "stereo"
   },
   {
      "genesis_plus_gx_psg_preamp",
      "Nível de pré-amplificação do PSG",
      "Define o nível de pré-amplificação do áudio do Gerador de Som Programável (PSG) de 4 canais SN76496 emulado encontrado no Master System, Game Gear e Mega Drive.",
      {
         { NULL, NULL },
      },
      "150"
   },
   {
      "genesis_plus_gx_fm_preamp",
      "Nível de pré-amplificação do FM",
      "Define o nível de pré-amplificação de áudio da Unidade de Som FM emulada do Master System.",
      {
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_filter",
      "Filtro de áudio",
      "Ativa um filtro de áudio passa-baixo para simular melhor o som característico de um Mega Drive Modelo 1.",
      {
         { "disabled", NULL },
         { "low-pass", "Passa-baixo" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lowpass_range",
      "Filtro passa-baixo %",
      "Especifica a frequência de corte do filtro de áudio passa-baixo. Um valor mais alto aumenta a 'força' percebida do filtro, uma vez que uma faixa mais ampla do espectro de alta frequência é atenuada.",
      {
         { NULL, NULL },
      },
      "60"
   },
#ifdef HAVE_EQ
   {
      "genesis_plus_gx_audio_eq_low",
      "EQ baixa",
      "Ajuste a banda de alcance baixo do equalizador de áudio interno.",
      {
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_mid",
      "EQ média",
      "Ajuste a banda de alcance média do equalizador de áudio interno.",
      {
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_high",
      "EQ alta",
      "Ajuste a banda de alcance alta do equalizador de áudio interno.",
      {
         { NULL, NULL },
      },
      "100"
   },
#endif
   {
      "genesis_plus_gx_blargg_ntsc_filter",
      "Filtro Blargg NTSC",
      "Aplica um filtro de vídeo para imitar vários sinais de TV NTSC.",
      {
         { "disabled",   NULL },
         { "monochrome", "Monocromático" },
         { "composite",  "Composto" },
         { "svideo",     "S-Video" },
         { "rgb",        "RGB" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lcd_filter",
      "Filtro fantasma LCD",
      "Aplica um filtro de imagem fantasma para imitar as características de exibição dos painéis de LCD do Game Gear e do 'Genesis Nomad'.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_overscan",
      "Bordas",
      "Ative esta opção para exibir as regiões de overscan na parte superior/inferior e/ou esquerda/direita da tela. Normalmente, seriam ocultas pelo painel ao redor da borda de uma televisão de definição padrão.",
      {
         { "disabled",   NULL },
         { "top/bottom", "Superior/inferior" },
         { "left/right", "Esquerda/direita" },
         { "full",       "Completa" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_gg_extra",
      "Tela estendida do Game Gear",
      "Força os títulos do Game Gear a serem executados no modo 'SMS', com uma resolução aumentada de 256x192. Pode mostrar conteúdo adicional, mas geralmente exibe uma borda de dados corrompidos/indesejados de imagem.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_aspect_ratio",
      "Proporção de aspecto fornecida pelo núcleo",
      "Escolhe a proporção preferida do conteúdo. Isso se aplicará somente quando a proporção do RetroArch estiver configurada como 'Core provided' nas configurações de vídeo.",
      {
         { "auto",     NULL },
         { "NTSC PAR", NULL },
         { "PAL PAR",  NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_render",
      "Saída entrelaçada modo 2",
      "O Modo entrelaçado 2 permite que o Mega Drive produza uma imagem de 320x448 de altura dupla (alta resolução) desenhando linhas de varredura alternativas em cada quadro (isso é usado pelos modos multijogador do 'Sonic the Hedgehog 2' e 'Combat Cars'). O 'Campo duplo' imita o hardware original, produzindo uma imagem nítida com artefatos trêmulos/entrelaçados. O 'Campo único' é um filtro de desentrelaçamento, que estabiliza a imagem, mas causa desfoque suave.",
      {
         { "single field", "Campo único" },
         { "double field", "Campo duplo" },
         { NULL, NULL },
      },
      "single field"
   },
   {
      "genesis_plus_gx_gun_cursor",
      "Mostrar mira da pistola",
      "Exibe a mira da pistola de luz ao usar os tipos de dispositivo de entrada 'MD Menacer', 'MD Justifiers' e 'MS Light Phaser'.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_gun_input",
      "Saída da Pistola de luz",
      "Usa uma entrada 'Pistola de luz' ou 'Toque na tela' controlada por mouse.",
      {
         { "lightgun",    "Pistola de luz" },
         { "touchscreen", "Toque na tela" },
         { NULL, NULL },
      },
      "lightgun"
   },
   {
      "genesis_plus_gx_invert_mouse",
      "Inverter eixo Y do mouse",
      "Inverte o eixo Y do tipo de dispositivo de entrada 'MD Mouse'.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
#ifdef HAVE_OVERCLOCK
   {
      "genesis_plus_gx_overclock",
      "Velocidade da CPU",
      "Faz um overclock da CPU emulada. Pode reduzir a desaceleração, mas pode causar falhas.",
      {
         { NULL, NULL },
      },
      "100%"
   },
#endif
   {
      "genesis_plus_gx_no_sprite_limit",
      "Remover limite de sprite por linha",
      "Remove o limite de sprite por varredura do hardware, 8 (Master System) ou 20 (Mega Drive). Isso reduz a tremulação (flickering), mas pode causar falhas visuais, pois alguns jogos exploram o limite de hardware para gerar efeitos especiais.",
      {
         { NULL, NULL },
      },
      "disabled"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

	struct retro_core_option_definition option_defs_tr[] = {
   {
      "genesis_plus_gx_system_hw",
      "Sistem Donanımı",
      "Yüklenen içeriği belirli bir öykünmüş konsolla çalıştırır. 'Otomatik' mevcut oyun için en uygun sistemi seçecektir.",
      {
         { "auto",                 "Otomatik"               },
         { "sg-1000",              "SG-1000"            },
         { "sg-1000 II",           "SG-1000 II"         },
         { "mark-III",             "Mark III"           },
         { "master system",        "Master System"      },
         { "master system II",     "Master System II"   },
         { "game gear",            "Game Gear"          },
         { "mega drive / genesis", "Mega Drive/Genesis" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_region_detect",
      "Sistem Bölgesi",
      "Sistemin hangi bölgeden olduğunu belirtin. Game Gear dışındaki konsollar için 'PAL' 50hz, 'NTSC' 60hz'dir. Yanlış bölge seçiliyse, oyunlar normalden daha hızlı veya daha yavaş çalışabilir.",
      {
         { "auto",    "Otomatik"   },
         { "ntsc-u",  "NTSC-U" },
         { "pal",     "PAL"    },
         { "ntsc-j",  "NTSC-J" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_force_dtack",
      "Sistem Kilidi",
      "Geçersiz adres erişimi gerçekleştirirken gerçek donanımda meydana gelen sistem kilitlemelerine öykünün. Bu, yalnızca doğru işlem için yasadışı davranışa dayanan belirli demolar ve homebrew oynatılırken devre dışı bırakılmalıdır.",
      {
         { "enabled",  "Etkinleştir" },
         { "disabled",  "Devre Dışı" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_bios",
      "Sistem Önyükleme ROM'u",
      "RetroArch'ın sistem dizininde varsa, öykünülmüş donanım için resmi BIOS/önyükleyici kullanın. Konsola özgü başlangıç sırası/animasyonu görüntüler, ardından yüklü içeriği çalıştırır.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_bram",
      "CD Sistemi BRAM'i",
      "Sega CD içeriği çalıştırılırken, belirli bir bölgedeki tüm oyunlar arasında (BIOS başına) tek bir kayıt dosyasının paylaşılmasını mı yoksa her oyun için ayrı bir kayıt dosyası oluşturup oluşturmamayı (Oyun Başına) belirtir. Sega CD'sinin sınırlı bir dahili depolama alanına sahip olduğunu ve yalnızca bir avuç başlık için yeterli olduğunu unutmayın. Boşluktan kaçınmak için 'Oyun Başına' ayarı önerilir.",
      {
         { "per bios", "BIOS Başına" },
         { "per game", "Oyun Başına" },
         { NULL, NULL },
      },
      "per bios"
   },
   {
      "genesis_plus_gx_addr_error",
      "68K Adres Hatası",
      "Genesis CPU (Motorola 68000), hizalanmamış hafıza erişimi gerçekleştirmeye çalışırken bir Adres Hatası (kilitlenme) üretir. '68K Adres Hatası' özelliğini etkinleştirmek bu davranışı simüle eder. ROM hacklerini oynarken sadece devre dışı bırakılmalıdır, çünkü bunlar daha az doğru emülatörler kullanılarak geliştirilir ve doğru işlem için geçersiz RAM erişimine güvenebilir.",
      {
         { "enabled",  "Etkinleştir" },
         { "disabled",  "Devre Dışı" },
         { NULL, NULL },
      },
      "enabled"
   },
   {
      "genesis_plus_gx_lock_on",
      "Kartuş Kilitleme",
      "Kilitleme Teknolojisi, eski bir oyunun genişletilmiş veya değiştirilmiş bir oyun için özel bir kartuşun geçiş portuna bağlanmasına izin veren bir Genesis özelliğidir. Bu seçenek, hangi tür 'özel kilitleme' kartuşunun taklit edileceğini belirler. RetroArch'ın sistem dizininde ilgili bir bios dosyası bulunmalıdır.",
      {
         { "disabled",            "Devre Dışı" },
         { "game genie",          "Game Genie" },
         { "action replay (pro)", "Action Replay (Pro)" },
         { "sonic & knuckles",    "Sonic & Knuckles" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_ym2413",
      "Master System FM (YM2413)",
      "Gelişmiş ses çıkışı için bazı Sega Mark III / Master System oyunları tarafından kullanılan FM Ses Ünitesinin emülasyonunu etkinleştirin.",
      {
         { "auto",     "Otomatik" },
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_ym2612",
      "Mega Drive / Genesis FM",
#ifdef HAVE_YM3438_CORE
      "Mega Drive / Genesis'in FM sentezleyicisini (ana ses üreteci) taklit etmek için kullanılan yöntemi seçin. 'MAME' seçenekleri hızlı ve çoğu sistemde tam hızda çalışıyor. 'Nuke' seçenekleri döngüde doğrudur, çok kaliteli ve önemli CPU gereksinimlerine sahiptir. 'YM2612' yongası, orijinal Model 1 Genesis tarafından kullanılır. 'YM3438' daha sonra Genesis revizyonlarında kullanılır.",
#else
      "Mega Drive / Genesis'in FM sentezleyicisini (ana ses üreteci) taklit etmek için kullanılan yöntemi seçin. 'YM2612' yongası, orijinal Model 1 Genesis tarafından kullanılır. 'YM3438' daha sonra Genesis revizyonlarında kullanılır.",
#endif
      {
         { "mame (ym2612)",          "MAME (YM2612)" },
         { "mame (asic ym3438)",     "MAME (ASIC YM3438)" },
         { "mame (enhanced ym3438)", "MAME (Enhanced YM3438)" },
#ifdef HAVE_YM3438_CORE
         { "nuked (ym2612)",         "Nuked (YM2612)" },
         { "nuked (ym3438)",         "Nuked (YM3438)" },
#endif
         { NULL, NULL },
      },
      "mame (ym2612)"
   },
   {
      "genesis_plus_gx_sound_output",
      "Ses Çıkışı",
      "Stereo veya mono ses üretimini seçin.",
      {
         { "stereo", "Stereo" },
         { "mono",   "Mono" },
         { NULL, NULL },
      },
      "stereo"
   },
   {
      "genesis_plus_gx_psg_preamp",
      "PSG Preamp Level",
      "Master System, Game Gear ve Genesis'de bulunan öykünmüş SN76496 4 kanallı Programlanabilir Ses Üretecinin ses ön yükselticisi seviyesini ayarlayın.",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { "105", NULL },
         { "110", NULL },
         { "115", NULL },
         { "120", NULL },
         { "125", NULL },
         { "130", NULL },
         { "135", NULL },
         { "140", NULL },
         { "145", NULL },
         { "150", NULL },
         { "155", NULL },
         { "160", NULL },
         { "165", NULL },
         { "170", NULL },
         { "175", NULL },
         { "180", NULL },
         { "185", NULL },
         { "190", NULL },
         { "195", NULL },
         { "200", NULL },
         { NULL, NULL },
      },
      "150"
   },
   {
      "genesis_plus_gx_fm_preamp",
      "FM Preamp Level",
      "Öykünülmüş Sega Mark III/Master System FM Ses Ünitesinin ses ön yükselticisi seviyesini ayarlayın.",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { "105", NULL },
         { "110", NULL },
         { "115", NULL },
         { "120", NULL },
         { "125", NULL },
         { "130", NULL },
         { "135", NULL },
         { "140", NULL },
         { "145", NULL },
         { "150", NULL },
         { "155", NULL },
         { "160", NULL },
         { "165", NULL },
         { "170", NULL },
         { "175", NULL },
         { "180", NULL },
         { "185", NULL },
         { "190", NULL },
         { "195", NULL },
         { "200", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_filter",
      "Ses Filtresi",
      "Model 1 Genesis'in karakteristik sesini daha iyi simüle etmek için düşük geçişli bir ses filtresini etkinleştirin.",
      {
         { "disabled",  "Devre Dışı" },
         { "low-pass", "Low-Pass" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lowpass_range",
      "Low-Pass Filtresi %",
      "Düşük ses geçiş filtresinin kesme frekansını belirtin. Daha yüksek bir değer, yüksek frekans spektrumunun daha geniş bir aralığı azaltıldığı için filtrenin algılanan gücünü arttırır.",
      {
         { "5",  NULL },
         { "10", NULL },
         { "15", NULL },
         { "20", NULL },
         { "25", NULL },
         { "30", NULL },
         { "35", NULL },
         { "40", NULL },
         { "45", NULL },
         { "50", NULL },
         { "55", NULL },
         { "60", NULL },
         { "65", NULL },
         { "70", NULL },
         { "75", NULL },
         { "80", NULL },
         { "85", NULL },
         { "90", NULL },
         { "95", NULL },
         { NULL, NULL },
      },
      "60"
   },
#ifdef HAVE_EQ
   {
      "genesis_plus_gx_audio_eq_low",
      "EQ Low",
      "Dahili ses ekolayzırın düşük aralık bandını ayarlayın.",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_mid",
      "EQ Mid",
      "Dahili ses ekolayzerinin orta aralık bandını ayarlayın.",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
   {
      "genesis_plus_gx_audio_eq_high",
      "EQ High",
      "Dahili ses ekolayzerinin yüksek aralık bandını ayarlayın.",
      {
         { "0",   NULL },
         { "5",   NULL },
         { "10",  NULL },
         { "15",  NULL },
         { "20",  NULL },
         { "25",  NULL },
         { "30",  NULL },
         { "35",  NULL },
         { "40",  NULL },
         { "45",  NULL },
         { "50",  NULL },
         { "55",  NULL },
         { "60",  NULL },
         { "65",  NULL },
         { "70",  NULL },
         { "75",  NULL },
         { "80",  NULL },
         { "85",  NULL },
         { "90",  NULL },
         { "95",  NULL },
         { "100", NULL },
         { NULL, NULL },
      },
      "100"
   },
#endif
   {
      "genesis_plus_gx_blargg_ntsc_filter",
      "Blargg NTSC Filtresi",
      "Çeşitli NTSC TV sinyallerini taklit etmek için bir video filtresi uygulayın.",
      {
         { "disabled",   "Devre Dışı" },
         { "monochrome", "Monochrome" },
         { "composite",  "Composite" },
         { "svideo",     "S-Video" },
         { "rgb",        "RGB" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_lcd_filter",
      "LCD Gölgelenme Filtresi",
      "Game Gear ve 'Genesis Nomad' LCD panellerinin ekran özelliklerini taklit etmek için bir görüntü “gölgelenme” filtresi uygulayın.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_overscan",
      "Çerçeveler",
      "Aşırı tarama bölgelerini ekranın üstünde/altında ve/veya sol/sağında görüntülemek için bunu etkinleştirin. Bunlar normalde standart tanımlı bir televizyonun kenarındaki çerçeve tarafından gizlenir.",
      {
         { "disabled",   "Devre Dışı" },
         { "top/bottom", "Üstünde/Atında" },
         { "left/right", "Sol/Sağ" },
         { "full",       "Tam" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_gg_extra",
      "Game Gear Genişletilmiş Ekran",
      "Game Gear oyunlarını 256x192 çözünürlüğe sahip 'SMS' modunda çalıştırmaya zorlar. Ek içerik gösterebilir, ancak genellikle bozuk/istenmeyen resim verilerinin kenarlığını görüntüler.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_aspect_ratio",
      "Çekirdek Tarafından Sağlanan En Boy Oranı",
      "Tercih edilen içerik en boy oranını seçin. Bu, yalnızca RetroArch’ın en boy oranı Video ayarlarında 'Çekirdek Tarafından Sağlanan' olarak ayarlandığında uygulanacaktır.",
      {
         { "auto",     "Otomatik" },
         { "NTSC PAR", NULL },
         { "PAL PAR",  NULL },
      },
      "auto"
   },
   {
      "genesis_plus_gx_render",
      "Geçmeli Mod 2 Çıkışı",
      "Geçmeli Mod 2, Genesis'e, her kareye alternatif tarama çizgileri çizerek (bu, 'Sonic the Hedgehog 2' ve 'Savaş Arabaları' çok oyunculu modları tarafından kullanılır) çift yükseklikte (yüksek çözünürlüklü) 320x448 görüntü vermesini sağlar. 'Çift Alan' orijinal donanımı taklit eder, titreyen/birbirine geçen eserler ile keskin bir görüntü oluşturur. 'Tek Alan', görüntüyü dengeleyen ancak hafif bulanıklığa neden olan, birbirinin yerine geçen bir filtredir.",
      {
         { "single field", "Tek Alan" },
         { "double field", "Çift Alan" },
         { NULL, NULL },
      },
      "single field"
   },
   {
      "genesis_plus_gx_gun_cursor",
      "Light Gun Göstergesini Göster",
      "'MD Menacer', 'MD Justifiers' ve 'MS Light Phaser' giriş cihazı tiplerini kullanırken Light Gun göstergelerini gösterin.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "genesis_plus_gx_gun_input",
      "Light Gun Girdisi",
      "Fare kontrollü 'Light Gun' veya 'Dokunmatik Ekran' girişi kullanın.",
      {
         { "lightgun",    "Light Gun" },
         { "touchscreen", "Dokunmatik Ekran" },
         { NULL, NULL },
      },
      "lightgun"
   },
   {
      "genesis_plus_gx_invert_mouse",
      "Fare Y Eksenini Ters Çevir",
      "'MD Fare' giriş cihazı türünün Y eksenini ters çevirir.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
#ifdef HAVE_OVERCLOCK
   {
      "genesis_plus_gx_overclock",
      "CPU Hızı",
      "Öykünülmüş CPU hız aşırtması. Yavaşlamayı azaltabilir, ancak aksaklığa neden olabilir.",
      {
         { "100%", NULL },
         { "125%", NULL },
         { "150%", NULL },
         { "175%", NULL },
         { "200%", NULL },
         { NULL, NULL },
      },
      "100%"
   },
#endif
   {
      "genesis_plus_gx_no_sprite_limit",
      "Satır Başına Sprite Limitini Kaldır",
      "8 (Ana Sistem) veya 20 (Genesis) tarama başına sprite donanım sınırını kaldırır. Bu, titremeyi azaltır ancak bazı oyunlar özel efektler oluşturmak için donanım sınırını kullandığı için görsel hatalara neden olabilir.",
      {
         { "disabled",  "Devre Dışı" },
         { "enabled",  "Etkinleştir" },
         { NULL, NULL },
      },
      "disabled"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

#ifdef __cplusplus
}
#endif

#endif
