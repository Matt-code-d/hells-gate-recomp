using System;
using System.Collections.Generic;

namespace DantesInferno
{
    /// <summary>
    /// Provides localized strings for the launcher UI.
    /// Supported languages: English (en), Spanish (es), Portuguese (pt), French (fr).
    /// </summary>
    public static class LauncherLocalizer
    {
        public const string DefaultLanguage = "en";

        public static readonly List<string> SupportedLanguages = new List<string>
        {
            "en", "es", "pt", "fr"
        };

        public static readonly Dictionary<string, string> LanguageDisplayNames = new Dictionary<string, string>
        {
            { "en", "English" },
            { "es", "Espanol" },
            { "pt", "Portugues" },
            { "fr", "Francais" },
        };

        // String keys used throughout the launcher.
        public const string
            TabPlay = "tab_play",
            TabDlc = "tab_dlc",
            TabControls = "tab_controls",
            TabSettings = "tab_settings",
            TabUpdates = "tab_updates",
            PlaySubtitle = "play_subtitle",
            PlayStatusReady = "play_status_ready",
            PlayButton = "play_button",
            ExitButton = "exit_button",
            SupportDeveloper = "support_developer",
            VersionLabel = "version_label",
            DlcTitle = "dlc_title",
            DlcDescription = "dlc_description",
            OpenDlcFolder = "open_dlc_folder",
            OpenTuFolder = "open_tu_folder",
            DlcFolderEmpty = "dlc_folder_empty",
            DlcFolderFound = "dlc_folder_found",
            TuInstalled = "tu_installed",
            TuNotFound = "tu_not_found",
            ControlsTitle = "controls_title",
            ControlsHint = "controls_hint",
            GroupActions = "group_actions",
            GroupShoulders = "group_shoulders",
            GroupLeftStick = "group_left_stick",
            GroupRightStick = "group_right_stick",
            GroupDpad = "group_dpad",
            LabelA = "label_a",
            LabelB = "label_b",
            LabelX = "label_x",
            LabelY = "label_y",
            LabelBack = "label_back",
            LabelStart = "label_start",
            LabelLeftShoulder = "label_left_shoulder",
            LabelRightShoulder = "label_right_shoulder",
            LabelLeftTrigger = "label_left_trigger",
            LabelRightTrigger = "label_right_trigger",
            LabelUp = "label_up",
            LabelDown = "label_down",
            LabelLeft = "label_left",
            LabelRight = "label_right",
            LabelPressSprint = "label_press_sprint",
            LabelPress = "label_press",
            BindingsAppliedNote = "bindings_applied_note",
            ResetToDefault = "reset_to_default",
            SaveBindings = "save_bindings",
            GroupGraphics = "group_graphics",
            LabelDisplay = "label_display",
            LabelResolution = "label_resolution",
            LabelRenderer = "label_renderer",
            LabelAntiAliasing = "label_anti_aliasing",
            LabelTextureFiltering = "label_texture_filtering",
            LabelGameLanguage = "label_game_language",
            LabelLauncherLanguage = "label_launcher_language",
            CheckFullscreen = "check_fullscreen",
            GroupControls = "group_controls",
            CheckControllerBackend = "check_controller_backend",
            LabelButtonGlyphs = "label_button_glyphs",
            ComingSoon = "coming_soon",
            CheckLogging = "check_logging",
            SettingsAppliedNote = "settings_applied_note",
            ResetToRecommended = "reset_to_recommended",
            SaveSettings = "save_settings",
            UpdatesTitle = "updates_title",
            InstalledVersion = "installed_version",
            CheckForUpdates = "check_for_updates",
            DownloadInstall = "download_install",
            PlayStatusMissing = "play_status_missing",
            TrialsServerTitle = "trials_server_title",
            TrialsServerMessage = "trials_server_message",
            TrialsServerButton = "trials_server_button",
            ResolutionTooltip = "resolution_tooltip",
            RendererTooltip = "renderer_tooltip",
            LanguageGameTooltip = "language_game_tooltip",
            LanguageLauncherTooltip = "language_launcher_tooltip",
            TuFolderTooltip = "tu_folder_tooltip",
            ControllerTooltip = "controller_tooltip",
            GlyphTooltip = "glyph_tooltip";

        private static readonly Dictionary<string, Dictionary<string, string>> _translations;

        static LauncherLocalizer()
        {
            _translations = new Dictionary<string, Dictionary<string, string>>();

            // English
            var en = new Dictionary<string, string>
            {
                { TabPlay, "PLAY" },
                { TabDlc, "DLC" },
                { TabControls, "CONTROLS" },
                { TabSettings, "SETTINGS" },
                { TabUpdates, "UPDATES" },
                { PlaySubtitle, "Ready to enter the Nine Circles." },
                { PlayStatusReady, "Game files found." },
                { PlayStatusMissing, "Game files missing. Run the installer first." },
                { TrialsServerTitle, "Trials of Saint Lucia - Online Server" },
                { TrialsServerMessage, "A dedicated server for the Trials of Saint Lucia DLC is currently in development and will launch in the coming days or weeks. Due to server costs, access will be limited to Ko-Fi members for now. Keep an eye on our Discord for the launch announcement." },
                { TrialsServerButton, "Support on Ko-Fi" },
                { PlayButton, "PLAY" },
                { ExitButton, "EXIT" },
                { SupportDeveloper, "Support the Developer" },
                { VersionLabel, "Version: {0}" },
                { DlcTitle, "Downloadable Content" },
                { DlcDescription, "To install DLC, copy your DLC STFS package files into the DLC folder, then launch the game. The game will automatically install them on startup." },
                { OpenDlcFolder, "Open DLC Folder" },
                { OpenTuFolder, "Open TU Folder" },
                { DlcFolderEmpty, "DLC folder: empty" },
                { DlcFolderFound, "DLC folder: {0} file(s) found" },
                { TuInstalled, "Title Update (default.xexp): installed" },
                { TuNotFound, "Title Update (default.xexp): not found" },
                { ControlsTitle, "Keyboard Bindings" },
                { ControlsHint, "Click a field and press a key to bind it. Use combinations like Shift+Up for D-pad." },
                { GroupActions, "Actions" },
                { GroupShoulders, "Shoulders / Triggers" },
                { GroupLeftStick, "Left Stick (Movement)" },
                { GroupRightStick, "Right Stick (Camera)" },
                { GroupDpad, "D-Pad" },
                { LabelA, "A (Interact):" },
                { LabelB, "B (Cancel):" },
                { LabelX, "X (Attack):" },
                { LabelY, "Y (Magic):" },
                { LabelBack, "Back:" },
                { LabelStart, "Start:" },
                { LabelLeftShoulder, "Left Shoulder:" },
                { LabelRightShoulder, "Right Shoulder:" },
                { LabelLeftTrigger, "Left Trigger:" },
                { LabelRightTrigger, "Right Trigger:" },
                { LabelUp, "Up:" },
                { LabelDown, "Down:" },
                { LabelLeft, "Left:" },
                { LabelRight, "Right:" },
                { LabelPressSprint, "Press (Sprint):" },
                { LabelPress, "Press:" },
                { BindingsAppliedNote, "Bindings are applied when you click PLAY." },
                { ResetToDefault, "Reset to Default" },
                { SaveBindings, "Save Bindings" },
                { GroupGraphics, "Graphics Quality" },
                { LabelDisplay, "Display:" },
                { LabelResolution, "Resolution:" },
                { LabelRenderer, "Renderer:" },
                { LabelAntiAliasing, "Anti-Aliasing:" },
                { LabelTextureFiltering, "Texture Filtering:" },
                { LabelGameLanguage, "Game Language:" },
                { LabelLauncherLanguage, "Launcher Language:" },
                { CheckFullscreen, "Fullscreen" },
                { GroupControls, "Controls" },
                { CheckControllerBackend, "Use SDL controller backend (Xbox / PlayStation controllers)" },
                { LabelButtonGlyphs, "Button glyphs:" },
                { ComingSoon, "Coming soon" },
                { CheckLogging, "Enable logging (for crash reports)" },
                { SettingsAppliedNote, "Settings are applied when you click PLAY." },
                { ResetToRecommended, "Reset to Recommended" },
                { SaveSettings, "Save Settings" },
                { UpdatesTitle, "Check for Updates" },
                { InstalledVersion, "Installed version: {0}" },
                { CheckForUpdates, "Check for Updates" },
                { DownloadInstall, "Download & Install" },
                { ResolutionTooltip, "Internal render resolution. 'Auto' matches your display height. Other values are display-equivalent; the game renders at 16:9 pixels and is stretched to your monitor's aspect ratio." },
                { RendererTooltip, "ReXGlue = original D3D12 plugin renderer. Native = in-process Vulkan Xenos renderer." },
                { LanguageGameTooltip, "Game language. Detected from the XEX game region. English is always available." },
                { LanguageLauncherTooltip, "Language for the launcher interface." },
                { TuFolderTooltip, "Opens the game folder where default.xexp (Title Update) must be placed." },
                { ControllerTooltip, "Checked = SDL3 (all controller families). Unchecked = XInput (Xbox pads only)." },
                { GlyphTooltip, "Coming soon" },
            };
            _translations["en"] = en;

            // Spanish
            var es = new Dictionary<string, string>
            {
                { TabPlay, "JUGAR" },
                { TabDlc, "DLC" },
                { TabControls, "CONTROLES" },
                { TabSettings, "AJUSTES" },
                { TabUpdates, "ACTUALIZ." },
                { PlaySubtitle, "Listo para entrar a los Nueve Circulos." },
                { PlayStatusReady, "Archivos del juego encontrados." },
                { PlayStatusMissing, "Faltan archivos del juego. Ejecuta el instalador primero." },
                { TrialsServerTitle, "Trials of Saint Lucia - Servidor Online" },
                { TrialsServerMessage, "Un servidor dedicado para el DLC Trials of Saint Lucia esta actualmente en desarrollo y se lanzara en los proximos dias o semanas. Debido a los costos del servidor, el acceso estara limitado a miembros de Ko-Fi por ahora. Esten atentos a nuestro Discord para el anuncio del lanzamiento." },
                { TrialsServerButton, "Apoyar en Ko-Fi" },
                { PlayButton, "JUGAR" },
                { ExitButton, "SALIR" },
                { SupportDeveloper, "Apoya al Desarrollador" },
                { VersionLabel, "Version: {0}" },
                { DlcTitle, "Contenido Descargable" },
                { DlcDescription, "Para instalar DLC, copia tus archivos de paquete STFS de DLC en la carpeta DLC, luego inicia el juego. El juego los instalara automaticamente al arrancar." },
                { OpenDlcFolder, "Abrir Carpeta DLC" },
                { OpenTuFolder, "Abrir Carpeta TU" },
                { DlcFolderEmpty, "Carpeta DLC: vacia" },
                { DlcFolderFound, "Carpeta DLC: {0} archivo(s) encontrado(s)" },
                { TuInstalled, "Actualizacion (default.xexp): instalada" },
                { TuNotFound, "Actualizacion (default.xexp): no encontrada" },
                { ControlsTitle, "Asignaciones de Teclado" },
                { ControlsHint, "Haz clic en un campo y presiona una tecla para asignarla. Usa combinaciones como Shift+Up para el D-pad." },
                { GroupActions, "Acciones" },
                { GroupShoulders, "Hombros / Gatillos" },
                { GroupLeftStick, "Stick Izquierdo (Movimiento)" },
                { GroupRightStick, "Stick Derecho (Camara)" },
                { GroupDpad, "D-Pad" },
                { LabelA, "A (Interactuar):" },
                { LabelB, "B (Cancelar):" },
                { LabelX, "X (Atacar):" },
                { LabelY, "Y (Magia):" },
                { LabelBack, "Atras:" },
                { LabelStart, "Inicio:" },
                { LabelLeftShoulder, "Hombro Izq.:" },
                { LabelRightShoulder, "Hombro Der.:" },
                { LabelLeftTrigger, "Gatillo Izq.:" },
                { LabelRightTrigger, "Gatillo Der.:" },
                { LabelUp, "Arriba:" },
                { LabelDown, "Abajo:" },
                { LabelLeft, "Izquierda:" },
                { LabelRight, "Derecha:" },
                { LabelPressSprint, "Presionar (Correr):" },
                { LabelPress, "Presionar:" },
                { BindingsAppliedNote, "Las asignaciones se aplican al hacer clic en JUGAR." },
                { ResetToDefault, "Restaurar Predeterminado" },
                { SaveBindings, "Guardar Asignaciones" },
                { GroupGraphics, "Calidad Grafica" },
                { LabelDisplay, "Pantalla:" },
                { LabelResolution, "Resolucion:" },
                { LabelRenderer, "Renderizador:" },
                { LabelAntiAliasing, "Anti-Aliasing:" },
                { LabelTextureFiltering, "Filtrado de Textura:" },
                { LabelGameLanguage, "Idioma del Juego:" },
                { LabelLauncherLanguage, "Idioma del Launcher:" },
                { CheckFullscreen, "Pantalla Completa" },
                { GroupControls, "Controles" },
                { CheckControllerBackend, "Usar backend de controlador SDL (Xbox / PlayStation)" },
                { LabelButtonGlyphs, "Glifos de botones:" },
                { ComingSoon, "Proximamente" },
                { CheckLogging, "Habilitar registro (para informes de fallos)" },
                { SettingsAppliedNote, "Los ajustes se aplican al hacer clic en JUGAR." },
                { ResetToRecommended, "Restaurar Recomendado" },
                { SaveSettings, "Guardar Ajustes" },
                { UpdatesTitle, "Buscar Actualizaciones" },
                { InstalledVersion, "Version instalada: {0}" },
                { CheckForUpdates, "Buscar Actualizaciones" },
                { DownloadInstall, "Descargar e Instalar" },
                { ResolutionTooltip, "Resolucion de renderizado interno. 'Auto' coincide con la altura de tu pantalla. Los demas valores son equivalentes de pantalla; el juego renderiza a 16:9 y se estira a la relacion de aspecto de tu monitor." },
                { RendererTooltip, "ReXGlue = renderizador D3D12 original. Native = renderizador in-process Vulkan Xenos." },
                { LanguageGameTooltip, "Idioma del juego. Detectado desde la region XEX del juego. Ingles siempre disponible." },
                { LanguageLauncherTooltip, "Idioma para la interfaz del launcher." },
                { TuFolderTooltip, "Abre la carpeta del juego donde default.xexp (Actualizacion) debe colocarse." },
                { ControllerTooltip, "Marcado = SDL3 (todas las familias de controladores). Desmarcado = XInput (solo pads Xbox)." },
                { GlyphTooltip, "Proximamente" },
            };
            _translations["es"] = es;

            // Portuguese
            var pt = new Dictionary<string, string>
            {
                { TabPlay, "JOGAR" },
                { TabDlc, "DLC" },
                { TabControls, "CONTROLES" },
                { TabSettings, "AJUSTES" },
                { TabUpdates, "ATUALIZ." },
                { PlaySubtitle, "Pronto para entrar nos Nove Circulos." },
                { PlayStatusReady, "Arquivos do jogo encontrados." },
                { PlayStatusMissing, "Arquivos do jogo ausentes. Execute o instalador primeiro." },
                { TrialsServerTitle, "Trials of Saint Lucia - Servidor Online" },
                { TrialsServerMessage, "Um servidor dedicado para o DLC Trials of Saint Lucia esta atualmente em desenvolvimento e sera lancado nos proximos dias ou semanas. Devido aos custos do servidor, o acesso sera limitado a membros do Ko-Fi por enquanto. Fiquem atentos ao nosso Discord para o anuncio do lancamento." },
                { TrialsServerButton, "Apoiar no Ko-Fi" },
                { PlayButton, "JOGAR" },
                { ExitButton, "SAIR" },
                { SupportDeveloper, "Apoiar o Desenvolvedor" },
                { VersionLabel, "Versao: {0}" },
                { DlcTitle, "Conteudo Baixavel" },
                { DlcDescription, "Para instalar DLC, copie seus arquivos de pacote STFS de DLC para a pasta DLC e inicie o jogo. O jogo os instalara automaticamente na inicializacao." },
                { OpenDlcFolder, "Abrir Pasta DLC" },
                { OpenTuFolder, "Abrir Pasta TU" },
                { DlcFolderEmpty, "Pasta DLC: vazia" },
                { DlcFolderFound, "Pasta DLC: {0} arquivo(s) encontrado(s)" },
                { TuInstalled, "Atualizacao (default.xexp): instalada" },
                { TuNotFound, "Atualizacao (default.xexp): nao encontrada" },
                { ControlsTitle, "Atribuicoes de Teclado" },
                { ControlsHint, "Clique em um campo e pressione uma tecla para atribuir. Use combinacoes como Shift+Up para o D-pad." },
                { GroupActions, "Acoes" },
                { GroupShoulders, "Ombros / Gatilhos" },
                { GroupLeftStick, "Stick Esquerdo (Movimento)" },
                { GroupRightStick, "Stick Direito (Camera)" },
                { GroupDpad, "D-Pad" },
                { LabelA, "A (Interagir):" },
                { LabelB, "B (Cancelar):" },
                { LabelX, "X (Atacar):" },
                { LabelY, "Y (Magia):" },
                { LabelBack, "Voltar:" },
                { LabelStart, "Inicio:" },
                { LabelLeftShoulder, "Ombro Esq.:" },
                { LabelRightShoulder, "Ombro Dir.:" },
                { LabelLeftTrigger, "Gatilho Esq.:" },
                { LabelRightTrigger, "Gatilho Dir.:" },
                { LabelUp, "Cima:" },
                { LabelDown, "Baixo:" },
                { LabelLeft, "Esquerda:" },
                { LabelRight, "Direita:" },
                { LabelPressSprint, "Pressionar (Correr):" },
                { LabelPress, "Pressionar:" },
                { BindingsAppliedNote, "As atribuicoes sao aplicadas ao clicar em JOGAR." },
                { ResetToDefault, "Restaurar Padrao" },
                { SaveBindings, "Salvar Atribuicoes" },
                { GroupGraphics, "Qualidade Grafica" },
                { LabelDisplay, "Tela:" },
                { LabelResolution, "Resolucao:" },
                { LabelRenderer, "Renderizador:" },
                { LabelAntiAliasing, "Anti-Aliasing:" },
                { LabelTextureFiltering, "Filtragem de Textura:" },
                { LabelGameLanguage, "Idioma do Jogo:" },
                { LabelLauncherLanguage, "Idioma do Launcher:" },
                { CheckFullscreen, "Tela Cheia" },
                { GroupControls, "Controles" },
                { CheckControllerBackend, "Usar backend de controle SDL (Xbox / PlayStation)" },
                { LabelButtonGlyphs, "Glifos de botoes:" },
                { ComingSoon, "Em breve" },
                { CheckLogging, "Habilitar registro (para relatorios de falhas)" },
                { SettingsAppliedNote, "Os ajustes sao aplicados ao clicar em JOGAR." },
                { ResetToRecommended, "Restaurar Recomendado" },
                { SaveSettings, "Salvar Ajustes" },
                { UpdatesTitle, "Verificar Atualizacoes" },
                { InstalledVersion, "Versao instalada: {0}" },
                { CheckForUpdates, "Verificar Atualizacoes" },
                { DownloadInstall, "Baixar e Instalar" },
                { ResolutionTooltip, "Resolucao de renderizacao interna. 'Auto' corresponde a altura da sua tela. Os outros valores sao equivalentes de tela; o jogo renderiza em 16:9 e e esticado para a proporcao do seu monitor." },
                { RendererTooltip, "ReXGlue = renderizador D3D12 original. Native = renderizador Vulkan Xenos in-process." },
                { LanguageGameTooltip, "Idioma do jogo. Detectado pela regiao XEX do jogo. Ingles sempre disponivel." },
                { LanguageLauncherTooltip, "Idioma para a interface do launcher." },
                { TuFolderTooltip, "Abre a pasta do jogo onde default.xexp (Atualizacao) deve ser colocado." },
                { ControllerTooltip, "Marcado = SDL3 (todas as familias de controles). Desmarcado = XInput (apenas controles Xbox)." },
                { GlyphTooltip, "Em breve" },
            };
            _translations["pt"] = pt;

            // French
            var fr = new Dictionary<string, string>
            {
                { TabPlay, "JOUER" },
                { TabDlc, "DLC" },
                { TabControls, "CONTROLES" },
                { TabSettings, "PARAMETRES" },
                { TabUpdates, "MAJ" },
                { PlaySubtitle, "Pret a entrer dans les Neuf Cercles." },
                { PlayStatusReady, "Fichiers du jeu trouves." },
                { PlayStatusMissing, "Fichiers du jeu manquants. Lancez l'installateur d'abord." },
                { TrialsServerTitle, "Trials of Saint Lucia - Serveur En Ligne" },
                { TrialsServerMessage, "Un serveur dedie pour le DLC Trials of Saint Lucia est actuellement en developpement et sera lance dans les prochains jours ou semaines. En raison des couts du serveur, l'acces sera limite aux membres Ko-Fi pour le moment. Surveillez notre Discord pour l'annonce du lancement." },
                { TrialsServerButton, "Soutenir sur Ko-Fi" },
                { PlayButton, "JOUER" },
                { ExitButton, "QUITTER" },
                { SupportDeveloper, "Soutenir le Developpeur" },
                { VersionLabel, "Version : {0}" },
                { DlcTitle, "Contenu Telechargeable" },
                { DlcDescription, "Pour installer le DLC, copiez vos fichiers de paquet STFS de DLC dans le dossier DLC, puis lancez le jeu. Le jeu les installera automatiquement au demarrage." },
                { OpenDlcFolder, "Ouvrir le Dossier DLC" },
                { OpenTuFolder, "Ouvrir le Dossier TU" },
                { DlcFolderEmpty, "Dossier DLC : vide" },
                { DlcFolderFound, "Dossier DLC : {0} fichier(s) trouve(s)" },
                { TuInstalled, "Mise a jour (default.xexp) : installee" },
                { TuNotFound, "Mise a jour (default.xexp) : introuvable" },
                { ControlsTitle, "Affectations Clavier" },
                { ControlsHint, "Cliquez sur un champ et appuyez sur une touche pour l'affecter. Utilisez des combinaisons comme Shift+Up pour le D-pad." },
                { GroupActions, "Actions" },
                { GroupShoulders, "Epaules / Gachettes" },
                { GroupLeftStick, "Stick Gauche (Deplacement)" },
                { GroupRightStick, "Stick Droit (Camera)" },
                { GroupDpad, "D-Pad" },
                { LabelA, "A (Interagir) :" },
                { LabelB, "B (Annuler) :" },
                { LabelX, "X (Attaquer) :" },
                { LabelY, "Y (Magie) :" },
                { LabelBack, "Retour :" },
                { LabelStart, "Start :" },
                { LabelLeftShoulder, "Epaule Gauche :" },
                { LabelRightShoulder, "Epaule Droite :" },
                { LabelLeftTrigger, "Gachette Gauche :" },
                { LabelRightTrigger, "Gachette Droite :" },
                { LabelUp, "Haut :" },
                { LabelDown, "Bas :" },
                { LabelLeft, "Gauche :" },
                { LabelRight, "Droite :" },
                { LabelPressSprint, "Appuyer (Sprint) :" },
                { LabelPress, "Appuyer :" },
                { BindingsAppliedNote, "Les affectations sont appliquees quand vous cliquez sur JOUER." },
                { ResetToDefault, "Restaurer par Defaut" },
                { SaveBindings, "Enregistrer les Affectations" },
                { GroupGraphics, "Qualite Graphique" },
                { LabelDisplay, "Affichage :" },
                { LabelResolution, "Resolution :" },
                { LabelRenderer, "Moteur de rendu :" },
                { LabelAntiAliasing, "Anti-Aliasing :" },
                { LabelTextureFiltering, "Filtrage des Textures :" },
                { LabelGameLanguage, "Langue du Jeu :" },
                { LabelLauncherLanguage, "Langue du Launcher :" },
                { CheckFullscreen, "Plein Ecran" },
                { GroupControls, "Controles" },
                { CheckControllerBackend, "Utiliser le backend de controleur SDL (Xbox / PlayStation)" },
                { LabelButtonGlyphs, "Glyphes de boutons :" },
                { ComingSoon, "Bientot disponible" },
                { CheckLogging, "Activer la journalisation (pour rapports de plantage)" },
                { SettingsAppliedNote, "Les parametres sont appliques quand vous cliquez sur JOUER." },
                { ResetToRecommended, "Restaurer les Parametres Recommandes" },
                { SaveSettings, "Enregistrer les Parametres" },
                { UpdatesTitle, "Verifier les Mises a Jour" },
                { InstalledVersion, "Version installee : {0}" },
                { CheckForUpdates, "Verifier les Mises a Jour" },
                { DownloadInstall, "Telecharger et Installer" },
                { ResolutionTooltip, "Resolution de rendu interne. 'Auto' correspond a la hauteur de votre ecran. Les autres valeurs sont des equivalents d'ecran; le jeu rend en 16:9 et est etire au format de votre moniteur." },
                { RendererTooltip, "ReXGlue = moteur de rendu D3D12 original. Native = moteur de rendu Vulkan Xenos in-process." },
                { LanguageGameTooltip, "Langue du jeu. Detectee depuis la region XEX du jeu. Anglais toujours disponible." },
                { LanguageLauncherTooltip, "Langue pour l'interface du launcher." },
                { TuFolderTooltip, "Ouvre le dossier du jeu ou default.xexp (Mise a Jour) doit etre place." },
                { ControllerTooltip, "Coche = SDL3 (toutes les familles de controleurs). Decoche = XInput (manettes Xbox uniquement)." },
                { GlyphTooltip, "Bientot disponible" },
            };
            _translations["fr"] = fr;
        }

        /// <summary>
        /// Returns the localized string for the given key, or the key itself if not found.
        /// </summary>
        public static string Get(string language, string key)
        {
            if (_translations.TryGetValue(language, out var lang) &&
                lang.TryGetValue(key, out var value) && !string.IsNullOrEmpty(value))
                return value;
            // Fallback to English
            if (_translations.TryGetValue(DefaultLanguage, out var en) &&
                en.TryGetValue(key, out var enValue))
                return enValue;
            return key;
        }

        /// <summary>
        /// Returns the localized string formatted with the given args.
        /// </summary>
        public static string Get(string language, string key, params object[] args)
        {
            string template = Get(language, key);
            try { return string.Format(template, args); }
            catch { return template; }
        }

        /// <summary>
        /// Normalizes a language code to one of the supported languages.
        /// </summary>
        public static string NormalizeLanguage(string language)
        {
            if (string.IsNullOrEmpty(language))
                return DefaultLanguage;
            string lang = language.ToLowerInvariant();
            if (SupportedLanguages.Contains(lang))
                return lang;
            // Handle full names
            foreach (var kvp in LanguageDisplayNames)
            {
                if (kvp.Value.Equals(language, StringComparison.OrdinalIgnoreCase))
                    return kvp.Key;
            }
            return DefaultLanguage;
        }
    }
}
