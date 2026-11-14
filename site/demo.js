(() => {
  "use strict";

  const demoApp = document.querySelector("#demo-app");
  if (!demoApp) return;

  const workspaces = {
    kergit: {
      name: "Kergit Lab",
      subtitle: "Prototype workspace",
      slug: "kergit-lab",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "genel", name: "genel", topic: "Kergit için ortak alan", unread: false },
            { id: "cpp-core", name: "cpp-core", topic: "Signaling, queues and protocol", unread: true },
            { id: "ui-lab", name: "ui-lab", topic: "Client polish and product experiments", unread: false },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "build-room", name: "Build room", participants: ["ece", "deniz"] },
            { id: "focus-room", name: "Focus room", participants: ["mert"] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "Building Kergit", role: "owner", color: ["#7c3aed", "#2563eb"] },
        { id: "ece", name: "Ece", initials: "EC", status: "online", activity: "In Build room", role: "admin", color: ["#0891b2", "#2563eb"], speaking: true, voice: { camera: true } },
        { id: "deniz", name: "Deniz", initials: "DZ", status: "online", activity: "In Build room", color: ["#059669", "#0d9488"], voice: { muted: true } },
        { id: "mert", name: "Mert", initials: "MT", status: "online", activity: "In Focus room", color: ["#c2410c", "#db2777"] },
        { id: "selin", name: "Selin", initials: "SL", status: "online", activity: "Reading #cpp-core", color: ["#be185d", "#7c3aed"] },
        { id: "arda", name: "Arda", initials: "AR", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
        { id: "irem", name: "İrem", initials: "İR", status: "offline", activity: "Offline", color: ["#475569", "#1e293b"] },
        { id: "kaan", name: "Kaan", initials: "KN", status: "offline", activity: "Offline", color: ["#4b5563", "#374151"] },
      ],
      messages: {
        genel: [
          {
            author: "Bilal",
            initials: "BK",
            time: "20:38",
            text: "Welcome to the interactive Kergit workspace. Everything in this window stays in your browser.",
            badge: "owner",
            color: ["#7c3aed", "#2563eb"],
            reactions: ["👋 4", "⚡ 2"],
          },
          {
            author: "Ece",
            initials: "EC",
            time: "20:41",
            text: "The new voice ownership path feels much clearer. One session owns voice, the other tabs stay synchronized.",
            color: ["#0891b2", "#2563eb"],
            reactions: ["✅ 3"],
          },
          {
            author: "Deniz",
            initials: "DZ",
            time: "20:43",
            text: "I pushed a pass on the channel layout. Try switching to #cpp-core, then join Build room.",
            color: ["#059669", "#0d9488"],
          },
        ],
        "cpp-core": [
          {
            author: "Bilal",
            initials: "BK",
            time: "18:12",
            text: "The outbound path now keeps authoritative state ahead of repairable real-time signals.",
            badge: "owner",
            color: ["#7c3aed", "#2563eb"],
            codeBlock: "P0  state mutation   → never drop\nP1  message delta    → preserve order\nP2  presence update  → repair via sync\nP3  typing signal    → drop under load",
            reactions: ["🧠 5", "⚙️ 3"],
          },
          {
            author: "Selin",
            initials: "SL",
            time: "18:18",
            text: "That makes reconnect behavior much easier to reason about. The snapshot remains the repair boundary.",
            color: ["#be185d", "#7c3aed"],
          },
          {
            author: "Mert",
            initials: "MT",
            time: "18:24",
            text: "And the protobuf envelope stays stable while each payload evolves independently.",
            color: ["#c2410c", "#db2777"],
            reactions: ["👍 2"],
          },
        ],
        "ui-lab": [
          {
            author: "Deniz",
            initials: "DZ",
            time: "16:04",
            text: "The compact member panel is working. On small screens it becomes a slide-over instead of shrinking chat.",
            color: ["#059669", "#0d9488"],
          },
          {
            author: "Ece",
            initials: "EC",
            time: "16:09",
            text: "Nice. I also like that simulated controls never trigger a browser permission prompt.",
            color: ["#0891b2", "#2563eb"],
            reactions: ["✨ 4"],
          },
        ],
      },
    },
    opensource: {
      name: "Open Source Türkiye",
      subtitle: "Community workspace",
      slug: "open-source-tr",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "duyurular", name: "duyurular", topic: "Community announcements", unread: true },
            { id: "projeler", name: "projeler", topic: "What everyone is building", unread: false },
            { id: "yardim", name: "yardım", topic: "Ask, learn, and unblock", unread: false },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "community-stage", name: "Community stage", participants: ["aylin"] },
            { id: "coffee-room", name: "Coffee room", participants: [] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "Showing Kergit", color: ["#7c3aed", "#2563eb"] },
        { id: "aylin", name: "Aylin", initials: "AY", status: "online", activity: "On Community stage", role: "host", color: ["#0d9488", "#2563eb"], speaking: true, voice: { screen: "slides" } },
        { id: "emre", name: "Emre", initials: "EM", status: "online", activity: "Browsing projects", color: ["#ea580c", "#ca8a04"] },
        { id: "zeynep", name: "Zeynep", initials: "ZY", status: "online", activity: "Online", color: ["#db2777", "#7c3aed"] },
        { id: "can", name: "Can", initials: "CN", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
        { id: "eda", name: "Eda", initials: "ED", status: "offline", activity: "Offline", color: ["#4b5563", "#1e293b"] },
      ],
      messages: {
        duyurular: [
          {
            author: "Aylin",
            initials: "AY",
            time: "09:30",
            text: "Demo workspace notice: these conversations are illustrative and are not real community messages.",
            badge: "host",
            color: ["#0d9488", "#2563eb"],
          },
          {
            author: "Bilal",
            initials: "BK",
            time: "09:42",
            text: "Kergit’s project page now lets visitors experience the interface before diving into the architecture.",
            color: ["#7c3aed", "#2563eb"],
            reactions: ["🚀 6"],
          },
        ],
        projeler: [
          {
            author: "Emre",
            initials: "EM",
            time: "14:10",
            text: "What makes this project interesting is the full path: browser UI, protobuf transport, C++ authority, and media orchestration.",
            color: ["#ea580c", "#ca8a04"],
          },
          {
            author: "Zeynep",
            initials: "ZY",
            time: "14:14",
            text: "The honest demo boundary helps too—interactive enough to understand, without pretending a public backend exists.",
            color: ["#db2777", "#7c3aed"],
            reactions: ["💜 3"],
          },
        ],
        yardim: [
          {
            author: "Bilal",
            initials: "BK",
            time: "12:07",
            text: "Start with docs/ARCHITECTURE.md, then follow VOICE_JOIN through the service and LiveKit adapter.",
            color: ["#7c3aed", "#2563eb"],
            codeBlock: "browser → Caddy → C++ core → LiveKit\n                     ↘ PostgreSQL / Redis",
          },
        ],
      },
    },
    nightshift: {
      name: "Night Shift",
      subtitle: "Friends and games",
      slug: "night-shift",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "lobby", name: "lobby", topic: "Tonight’s plans", unread: false },
            { id: "clips", name: "clips", topic: "Screenshots and good moments", unread: true },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "squad-one", name: "Squad one", participants: ["mira", "baran", "ozan"] },
            { id: "afk", name: "AFK", participants: [] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "In Kergit Lab", color: ["#7c3aed", "#2563eb"] },
        { id: "mira", name: "Mira", initials: "MR", status: "online", activity: "In Squad one", color: ["#db2777", "#7c3aed"], speaking: true, voice: { screen: "scene" } },
        { id: "baran", name: "Baran", initials: "BR", status: "online", activity: "In Squad one", color: ["#0284c7", "#0891b2"], voice: { camera: true } },
        { id: "ozan", name: "Ozan", initials: "OZ", status: "online", activity: "In Squad one", color: ["#65a30d", "#059669"], voice: { muted: true } },
        { id: "lale", name: "Lale", initials: "LL", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
      ],
      messages: {
        lobby: [
          {
            author: "Mira",
            initials: "MR",
            time: "22:03",
            text: "Squad one is open. The green speaking ring is simulated, but this is how the real client presents voice activity.",
            color: ["#db2777", "#7c3aed"],
            reactions: ["🎮 3"],
          },
          {
            author: "Baran",
            initials: "BR",
            time: "22:05",
            text: "Joining now. No microphone prompt in this demo—exactly as promised.",
            color: ["#0284c7", "#0891b2"],
          },
        ],
        clips: [
          {
            author: "Ozan",
            initials: "OZ",
            time: "23:18",
            text: "Attachments are intentionally left out of the static simulation, but the real client has an attachment tray and previews.",
            color: ["#65a30d", "#059669"],
          },
        ],
      },
    },
  };


  const SELF_ID = "bilal";
  const CONNECT_DELAY_MS = 650;
  const SPEAK_INTERVAL_MS = 1200;
  const CONTROLS_IDLE_MS = 3000;
  const GRID_PAGE_SIZE = 9;
  const MOBILE_QUERY = "(max-width: 760px)";
  const COMPACT_QUERY = "(max-width: 1120px)";

  // Real excerpt from app/commands/activity/JoinVoiceChannelCommand.cpp, shown
  // as the "shared screen" when you start a simulated screen share.
  const SHARED_CODE = [
    ["cm", "// app/commands/activity/JoinVoiceChannelCommand.cpp"],
    ["", "auto& voice_sessions = ctx.voice_service.sessions();"],
    ["", "const auto old_owner_session = voice_sessions.user_session(user_id);"],
    ["", "const auto old_channel = voice_sessions.user_channel(user_id);"],
    ["", ""],
    ["hl", "const bool taking_over ="],
    ["hl", "    old_owner_session.has_value() && *old_owner_session != requester_session_id;"],
    ["", "const bool switching_channel = old_owner_session.has_value() &&"],
    ["", "                               *old_owner_session == requester_session_id &&"],
    ["", "                               old_channel.has_value() && *old_channel != channel_id;"],
  ];

  const initialChannelByHub = {
    kergit: "genel",
    opensource: "duyurular",
    nightshift: "lobby",
  };

  const initialMessages = JSON.parse(JSON.stringify(
    Object.fromEntries(Object.entries(workspaces).map(([key, workspace]) => [key, workspace.messages]))
  ));

  const state = {
    hub: "kergit",
    channelByHub: { ...initialChannelByHub },
    messages: JSON.parse(JSON.stringify(initialMessages)),
    voice: null,
    viewingGrid: false,
    muted: false,
    deafened: false,
    camera: false,
    screen: false,
    watching: new Set(),
    focusedKey: null,
    stripHidden: false,
    speaking: new Set(),
    search: "",
    botTimer: null,
    toastTimer: null,
    connectTimer: null,
    speakTimer: null,
    idleTimer: null,
  };

  const $ = (selector) => document.querySelector(selector);

  const elements = {
    workspaceName: $("#workspace-name"),
    channelList: $("#channel-list"),
    channelSymbol: $("#channel-symbol"),
    channelName: $("#channel-name"),
    channelTopic: $("#channel-topic"),
    messageList: $("#message-list"),
    memberList: $("#member-list"),
    memberTotal: $("#member-total"),
    messageForm: $("#message-form"),
    messageInput: $("#message-input"),
    search: $("#demo-search"),
    voiceDock: $("#voice-dock"),
    voiceDockOpen: $("#voice-dock-open"),
    voiceDockStatus: $("#voice-dock-status"),
    voiceDockChannel: $("#voice-dock-channel"),
    voiceDockDetail: $("#voice-dock-detail"),
    mute: $("#demo-mute"),
    deafen: $("#demo-deafen"),
    leaveVoice: $("#voice-leave"),
    camera: $("#demo-camera"),
    screen: $("#demo-screen"),
    voiceGrid: $("#voice-grid"),
    voiceGridBody: $("#voice-grid-body"),
    stripToggle: $("#voice-strip-toggle"),
    gridMic: $("#grid-mic"),
    gridCamera: $("#grid-camera"),
    gridScreen: $("#grid-screen"),
    gridLeave: $("#grid-leave"),
    gridFullscreen: $("#grid-fullscreen"),
    reset: $("#demo-reset"),
    toast: $("#demo-toast"),
    typing: $("#typing-row"),
    popover: $("#demo-popover"),
    scrim: $("#demo-scrim"),
    channelsToggle: $("#channels-toggle"),
    membersToggle: $("#members-toggle"),
    address: $(".browser-address span:last-child"),
  };

  // ── Small DOM helpers ────────────────────────────────────────────────

  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function svgIcon(name, extraClass = "") {
    const ns = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(ns, "svg");
    svg.setAttribute("class", `icon ${extraClass}`.trim());
    svg.setAttribute("aria-hidden", "true");
    const use = document.createElementNS(ns, "use");
    use.setAttribute("href", `#icon-${name}`);
    svg.append(use);
    return svg;
  }

  function isMobile() {
    return window.matchMedia(MOBILE_QUERY).matches;
  }

  // ── Workspace / channel lookups ──────────────────────────────────────

  function currentWorkspace() {
    return workspaces[state.hub];
  }

  function currentChannelId() {
    return state.channelByHub[state.hub];
  }

  function findChannel(workspace, id) {
    return workspace.channels
      .flatMap((section) => section.items)
      .find((channel) => channel.id === id);
  }

  function currentChannel() {
    return findChannel(currentWorkspace(), currentChannelId());
  }

  function currentMessages() {
    return state.messages[state.hub][currentChannelId()] || [];
  }

  function selfMember() {
    return workspaces.kergit.members.find((member) => member.id === SELF_ID);
  }

  // ── Voice lookups ────────────────────────────────────────────────────

  function voiceWorkspace() {
    return state.voice ? workspaces[state.voice.hub] : null;
  }

  function voiceChannel() {
    return state.voice ? findChannel(voiceWorkspace(), state.voice.channel) : null;
  }

  function isVoiceConnected() {
    return state.voice?.phase === "connected";
  }

  function isInVoiceChannel(hubKey, channelId) {
    return Boolean(state.voice && state.voice.hub === hubKey && state.voice.channel === channelId);
  }

  // Mirrors the real client: the grid renders only while viewing it AND the
  // voice connection is established by this session.
  function isGridVisible() {
    return state.viewingGrid && isVoiceConnected() && state.voice.hub === state.hub;
  }

  function memberVoice(member) {
    return member.voice || {};
  }

  function remoteVoiceMembers() {
    const channel = voiceChannel();
    if (!channel) return [];
    const workspace = voiceWorkspace();
    return channel.participants
      .map((id) => workspace.members.find((member) => member.id === id))
      .filter(Boolean);
  }

  function isMemberMuted(member) {
    if (member.id === SELF_ID) return state.muted || state.deafened;
    return Boolean(memberVoice(member).muted);
  }

  // Speaking state is simulated only for the room you are connected to; other
  // rooms keep their static fixture flags.
  function speakerState(hubKey, channelId, member) {
    if (isVoiceConnected() && isInVoiceChannel(hubKey, channelId)) {
      return { live: true, speaking: state.speaking.has(member.id) };
    }
    return { live: false, speaking: Boolean(member.speaking) };
  }

  // Opening the grid starts with no remote camera or stream subscriptions;
  // leaving it drops them again (data-saving behavior of the real client).
  function setViewingGrid(next) {
    if (state.viewingGrid && !next) {
      state.watching.clear();
      state.focusedKey = null;
      state.stripHidden = false;
    }
    state.viewingGrid = next;
  }

  // ── Toast + popover ──────────────────────────────────────────────────

  function showToast(message) {
    window.clearTimeout(state.toastTimer);
    elements.toast.textContent = message;
    elements.toast.classList.add("visible");
    state.toastTimer = window.setTimeout(() => {
      elements.toast.classList.remove("visible");
    }, 2700);
  }

  function closePanels() {
    demoApp.classList.remove("channels-open", "members-open");
    elements.channelsToggle.setAttribute("aria-expanded", "false");
    if (window.matchMedia(COMPACT_QUERY).matches) {
      elements.membersToggle.setAttribute("aria-expanded", "false");
    }
    elements.popover.hidden = true;
  }

  function closePanelsOnMobile() {
    if (isMobile()) closePanels();
  }

  function showPopover(anchor, title, copy, actionLabel, onAction) {
    elements.popover.replaceChildren();

    const heading = el("h4", "", title);
    const description = el("p", "", copy);
    const action = el("button", "", actionLabel);
    action.type = "button";
    action.addEventListener("click", () => {
      elements.popover.hidden = true;
      onAction();
    }, { once: true });

    elements.popover.append(heading, description, action);
    elements.popover.hidden = false;

    const anchorRect = anchor.getBoundingClientRect();
    const appRect = demoApp.getBoundingClientRect();
    const desiredLeft = anchorRect.left - appRect.left + anchorRect.width + 10;
    const desiredTop = anchorRect.top - appRect.top;
    const maxLeft = Math.max(12, appRect.width - 294);
    const maxTop = Math.max(12, appRect.height - 150);
    elements.popover.style.left = `${Math.min(Math.max(12, desiredLeft), maxLeft)}px`;
    elements.popover.style.top = `${Math.min(Math.max(12, desiredTop), maxTop)}px`;
    action.focus({ preventScroll: true });
  }

  // ── Sidebar: channels ────────────────────────────────────────────────

  function renderVoiceUser(member, hubKey, channelId) {
    const isSelf = member.id === SELF_ID;
    const { live, speaking } = speakerState(hubKey, channelId, member);
    const row = el("div", "voice-user");
    if (live) row.dataset.speaker = member.id;
    row.classList.toggle("speaking", speaking);
    if (isSelf && state.voice?.phase === "connecting") row.classList.add("connecting");

    const avatar = el("span", "mini-avatar", member.initials);
    avatar.style.background = avatarStyle(member.color);
    const name = el("span", "voice-user-name", isSelf ? "You" : member.name);
    row.append(avatar, name);

    const icons = el("span", "voice-user-state");
    if (isSelf && state.deafened) {
      icons.append(svgIcon("headphones-off"), el("span", "sr-only", "deafened"));
    } else if (isMemberMuted(member)) {
      icons.append(svgIcon("mic-off"), el("span", "sr-only", "muted"));
    }
    const voice = isSelf ? { camera: state.camera, screen: state.screen } : memberVoice(member);
    if (voice.screen) {
      icons.append(el("span", "voice-user-live", "LIVE"));
    } else if (voice.camera) {
      icons.append(svgIcon("camera", "is-on"), el("span", "sr-only", "camera on"));
    }
    if (icons.childNodes.length) row.append(icons);
    return row;
  }

  function renderChannels() {
    const workspace = currentWorkspace();
    elements.channelList.replaceChildren();

    workspace.channels.forEach((section) => {
      const wrapper = el("section", "channel-section");

      const heading = el("div", "channel-section-head");
      const title = el("span", "", section.title);
      const add = el("button", "", "+");
      add.type = "button";
      add.setAttribute("aria-label", `Add to ${section.title}`);
      add.addEventListener("click", () => {
        showPopover(
          add,
          section.type === "voice" ? "Create voice channel" : "Create text channel",
          "This static demo keeps the creation flow local and temporary.",
          "Create a demo channel",
          () => showToast("Demo channel created for this preview — nothing was saved.")
        );
      });
      heading.append(title, add);
      wrapper.append(heading);

      section.items.forEach((channel) => {
        const button = el("button", "channel-button");
        button.type = "button";
        button.dataset.channel = channel.id;
        button.dataset.type = section.type;

        const inThisVoice = section.type === "voice" && isInVoiceChannel(state.hub, channel.id);
        const isTextActive = section.type === "text" && channel.id === currentChannelId() && !isGridVisible();
        const isVoiceActive = inThisVoice && isGridVisible();
        button.classList.toggle("active", isTextActive || isVoiceActive);
        button.classList.toggle("connected", inThisVoice);
        if (isTextActive || isVoiceActive) button.setAttribute("aria-current", "page");

        const icon = el("span", "channel-icon", section.type === "voice" ? "⌁" : "#");
        const name = el("span", "", channel.name);
        button.append(icon, name);

        if (section.type === "voice") {
          button.setAttribute("aria-label", inThisVoice
            ? `${channel.name}, connected — open voice grid`
            : `Join voice channel ${channel.name}`);
        }

        if (channel.unread && !isTextActive) {
          const unread = el("i", "channel-unread");
          unread.setAttribute("aria-label", "Unread");
          button.append(unread);
        }

        button.addEventListener("click", () => {
          if (section.type === "voice") {
            joinVoice(channel, button);
            return;
          }
          state.channelByHub[state.hub] = channel.id;
          state.search = "";
          elements.search.value = "";
          setViewingGrid(false);
          renderAll();
          closePanelsOnMobile();
        });
        wrapper.append(button);

        if (section.type === "voice") {
          const participants = channel.participants
            .map((id) => workspace.members.find((member) => member.id === id))
            .filter(Boolean);
          if (inThisVoice) participants.unshift(selfMember());
          if (participants.length) {
            const users = el("div", "voice-users");
            participants.forEach((member) => users.append(renderVoiceUser(member, state.hub, channel.id)));
            wrapper.append(users);
          }
        }
      });

      elements.channelList.append(wrapper);
    });
  }

  // ── Chat ─────────────────────────────────────────────────────────────

  function avatarStyle(colors) {
    const [start, end] = colors || ["#475569", "#1e293b"];
    return `linear-gradient(145deg, ${start}, ${end})`;
  }

  function createAvatar(initials, colors, className = "") {
    const avatar = el("span", `avatar ${className}`.trim(), initials);
    avatar.style.background = avatarStyle(colors);
    return avatar;
  }

  function renderMessage(message) {
    const row = el("article", `message ${message.local ? "is-local" : ""}`.trim());
    row.dataset.searchText = `${message.author} ${message.text} ${message.codeBlock || ""}`.toLowerCase();

    const avatar = createAvatar(message.initials, message.color);
    const copy = el("div", "message-copy");

    const meta = el("div", "message-meta");
    meta.append(el("strong", "", message.author));
    if (message.badge) meta.append(el("span", "message-badge", message.badge));
    meta.append(el("time", "", message.time));

    copy.append(meta, el("p", "", message.text));

    if (message.codeBlock) copy.append(el("pre", "message-codeblock", message.codeBlock));

    if (message.reactions?.length) {
      const reactions = el("div", "message-reactions");
      message.reactions.forEach((reaction) => reactions.append(el("span", "message-reaction", reaction)));
      copy.append(reactions);
    }

    row.append(avatar, copy);
    return row;
  }

  function renderMessages() {
    const channel = currentChannel();
    const messages = currentMessages();
    elements.messageList.replaceChildren();

    const welcome = el("div", "channel-welcome");
    welcome.append(
      el("span", "welcome-symbol", "#"),
      el("h3", "", `Welcome to #${channel.name}`),
      el("p", "", channel.topic)
    );
    elements.messageList.append(welcome, el("div", "date-divider", "Today"));

    const query = state.search.trim().toLowerCase();
    let shown = 0;
    messages.forEach((message) => {
      const row = renderMessage(message);
      if (query && !row.dataset.searchText.includes(query)) return;
      elements.messageList.append(row);
      shown += 1;
    });

    if (!shown) {
      const empty = el("div", "channel-welcome");
      empty.append(
        el("h3", "", "No matching messages"),
        el("p", "", "Try another phrase. Search is local to this demo channel.")
      );
      elements.messageList.append(empty);
    }

    window.requestAnimationFrame(() => {
      elements.messageList.scrollTop = elements.messageList.scrollHeight;
    });
  }

  // ── Members ──────────────────────────────────────────────────────────

  function memberActivity(member) {
    if (member.id === SELF_ID && state.voice) {
      return state.voice.phase === "connecting" ? `Joining ${state.voice.name}…` : `In ${state.voice.name}`;
    }
    return member.activity;
  }

  function renderMembers() {
    const members = currentWorkspace().members;
    const connectedHere = isVoiceConnected() && state.voice.hub === state.hub;
    const roomIds = new Set(connectedHere ? [SELF_ID, ...voiceChannel().participants] : []);
    elements.memberList.replaceChildren();
    elements.memberTotal.textContent = String(members.length);

    const groups = [
      ["Online", members.filter((member) => member.status === "online")],
      ["Offline", members.filter((member) => member.status === "offline")],
    ];

    groups.forEach(([label, groupMembers]) => {
      const group = el("section", "member-group");
      group.append(el("h3", "member-group-title", `${label} — ${groupMembers.length}`));

      groupMembers.forEach((member) => {
        const live = roomIds.has(member.id);
        const speaking = live ? state.speaking.has(member.id) : Boolean(member.speaking);
        const button = el("button", "member-item");
        button.type = "button";
        button.classList.toggle("offline", member.status === "offline");
        button.classList.toggle("speaking", speaking);
        if (live) button.dataset.speaker = member.id;

        const copy = el("span", "member-item-copy");
        const activity = memberActivity(member);
        copy.append(
          el("strong", "", member.id === SELF_ID ? `${member.name} (you)` : member.name),
          el("span", "", activity)
        );
        button.append(createAvatar(member.initials, member.color), copy);

        if (member.role) button.append(el("span", "member-role", member.role));

        button.addEventListener("click", () => {
          showPopover(
            button,
            member.name,
            `${activity}. This profile is illustrative demo data.`,
            "Mention in composer",
            () => {
              if (isGridVisible()) {
                setViewingGrid(false);
                renderAll();
              }
              elements.messageInput.value = `@${member.name} `;
              elements.messageInput.focus();
            }
          );
        });
        group.append(button);
      });

      elements.memberList.append(group);
    });
  }

  // ── Voice grid ───────────────────────────────────────────────────────

  function gridEntries() {
    if (!isVoiceConnected()) return [];
    const self = selfMember();
    const entries = [{ key: `p-${SELF_ID}`, type: "participant", member: self, isLocal: true }];
    if (state.screen) {
      entries.push({ key: `s-${SELF_ID}`, type: "screen", member: self, isLocal: true, scene: "code" });
    }
    remoteVoiceMembers().forEach((member) => {
      entries.push({ key: `p-${member.id}`, type: "participant", member, isLocal: false });
      const scene = memberVoice(member).screen;
      if (scene) entries.push({ key: `s-${member.id}`, type: "screen", member, isLocal: false, scene });
    });
    return entries;
  }

  function layoutClass(count) {
    if (count <= 1) return "layout-1";
    if (count === 2) return "layout-2";
    if (count <= 4) return "layout-4";
    if (count <= 6) return "layout-6";
    return "layout-9";
  }

  function entryHasVideo(entry) {
    if (entry.type === "screen") return entry.isLocal || state.watching.has(entry.key);
    if (entry.isLocal) return state.camera;
    return Boolean(memberVoice(entry.member).camera) && state.watching.has(entry.key);
  }

  function entryLabel(entry) {
    const name = entry.isLocal ? "your" : `${entry.member.name}’s`;
    return entry.type === "screen" ? `${name} screen` : (entry.isLocal ? "your tile" : entry.member.name);
  }

  function setWatching(entry, watching) {
    if (watching) state.watching.add(entry.key);
    else state.watching.delete(entry.key);
    renderVoiceGrid();
    const what = entry.type === "screen" ? "stream" : "camera";
    showToast(watching
      ? `Watching ${entry.member.name}’s ${what} — remote video is opt-in, so nothing downloads until you ask.`
      : `Stopped watching ${entry.member.name}’s ${what}.`);
  }

  function renderCameraFeed(entry) {
    const feed = el("div", "sim-video");
    const [start, end] = entry.member.color || ["#334155", "#0f172a"];
    feed.style.setProperty("--feed-a", start);
    feed.style.setProperty("--feed-b", end);
    feed.append(el("div", "sim-person"));
    return feed;
  }

  function renderScreenFeed(entry) {
    const feed = el("div", `sim-screen sim-screen-${entry.scene}`);

    if (entry.scene === "code") {
      const bar = el("div", "sim-window-bar");
      bar.append(el("i"), el("i"), el("i"), el("span", "", "JoinVoiceChannelCommand.cpp — kergit-server"));
      const code = el("pre", "sim-code");
      SHARED_CODE.forEach(([kind, text]) => {
        const line = el("span", kind ? `line ${kind}` : "line", text || " ");
        code.append(line);
      });
      feed.append(bar, code);
      return feed;
    }

    if (entry.scene === "slides") {
      const slide = el("div", "sim-slide");
      const list = el("ul");
      [
        "One session per user owns the active voice connection",
        "LiveKit tokens are minted by the server, never the browser",
        "Webhook events are reconciled, never trusted blindly",
      ].forEach((item) => list.append(el("li", "", item)));
      slide.append(
        el("small", "", "Kergit · voice lifecycle"),
        el("h5", "", "Voice is session-owned."),
        list
      );
      feed.append(slide, el("span", "sim-slide-count", "3 / 12"));
      return feed;
    }

    feed.append(
      el("span", "sim-sun"),
      el("span", "sim-floor"),
      el("span", "sim-hill sim-hill-a"),
      el("span", "sim-hill sim-hill-b"),
      el("span", "sim-hud", "ROUND 3 · 02:14")
    );
    return feed;
  }

  function renderTile(entry) {
    const { member } = entry;
    const isScreen = entry.type === "screen";
    const hasVideo = entryHasVideo(entry);
    const focused = state.focusedKey === entry.key;

    const outer = el("div", "grid-tile-outer");
    const tile = el("div", "grid-tile");
    tile.tabIndex = 0;
    tile.dataset.key = entry.key;
    tile.setAttribute("role", "button");
    tile.setAttribute("aria-label", focused ? `Return to grid from ${entryLabel(entry)}` : `Spotlight ${entryLabel(entry)}`);
    if (isScreen) {
      tile.classList.add("is-screen");
    } else {
      tile.dataset.speaker = member.id;
      tile.classList.toggle("speaking", state.speaking.has(member.id));
    }

    if (hasVideo) {
      tile.append(isScreen ? renderScreenFeed(entry) : renderCameraFeed(entry));
      tile.append(el("span", "tile-sim-chip", "simulated"));
    } else if (isScreen) {
      const placeholder = el("div", "tile-screen-placeholder");
      const watch = el("button", "watch-stream-btn", "Watch stream");
      watch.type = "button";
      watch.addEventListener("click", (event) => {
        event.stopPropagation();
        setWatching(entry, true);
      });
      placeholder.append(svgIcon("screen"), watch);
      tile.append(placeholder);
    } else {
      const avatarWrap = el("div", "tile-avatar");
      const avatar = el("span", "tile-avatar-circle", member.initials);
      avatar.style.background = avatarStyle(member.color);
      avatarWrap.append(avatar);
      if (!entry.isLocal && memberVoice(member).camera) {
        const optin = el("div", "camera-optin");
        const watch = el("button", "watch-stream-btn", "Watch camera");
        watch.type = "button";
        watch.addEventListener("click", (event) => {
          event.stopPropagation();
          setWatching(entry, true);
        });
        optin.append(el("span", "camera-badge", "Camera on"), watch);
        avatarWrap.append(optin);
      }
      tile.append(avatarWrap);
    }

    if (!entry.isLocal && hasVideo) {
      const stop = el("button", "tile-stop-btn", "Stop watching");
      stop.type = "button";
      stop.addEventListener("click", (event) => {
        event.stopPropagation();
        setWatching(entry, false);
      });
      tile.append(stop);
    }

    const overlay = el("div", "tile-overlay");
    if (isScreen) overlay.append(svgIcon("screen", "tile-screen-icon"));
    overlay.append(el("span", "tile-name", entry.isLocal ? `${member.name} (you)` : member.name));
    if (!isScreen) {
      if (entry.isLocal && state.deafened) {
        overlay.append(svgIcon("headphones-off", "tile-state-icon"), el("span", "sr-only", "deafened"));
      } else if (isMemberMuted(member)) {
        overlay.append(svgIcon("mic-off", "tile-state-icon"), el("span", "sr-only", "muted"));
      }
    }
    tile.append(overlay);

    tile.addEventListener("click", () => toggleFocus(entry.key));
    tile.addEventListener("keydown", (event) => {
      if (event.target !== tile) return;
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        toggleFocus(entry.key);
      }
    });

    outer.append(tile);
    return outer;
  }

  function renderVoiceGrid() {
    if (!isGridVisible()) return;
    const activeKey = document.activeElement?.closest?.(".grid-tile")?.dataset.key;
    const entries = gridEntries();
    if (state.focusedKey && !entries.some((entry) => entry.key === state.focusedKey)) {
      state.focusedKey = null;
    }

    const body = elements.voiceGridBody;
    body.replaceChildren();
    const spotlight = Boolean(state.focusedKey);
    elements.voiceGrid.classList.toggle("is-spotlight", spotlight);

    if (spotlight) {
      const wrap = el("div", "voice-spotlight");
      const main = el("div", "spotlight-main");
      main.append(renderTile(entries.find((entry) => entry.key === state.focusedKey)));
      wrap.append(main);
      const rest = entries.filter((entry) => entry.key !== state.focusedKey);
      if (rest.length && !state.stripHidden) {
        const strip = el("div", "spotlight-strip");
        rest.forEach((entry) => strip.append(renderTile(entry)));
        wrap.append(strip);
      }
      body.append(wrap);
    } else {
      const visible = entries.slice(0, GRID_PAGE_SIZE);
      const grid = el("div", `voice-grid ${layoutClass(visible.length)}`);
      visible.forEach((entry) => grid.append(renderTile(entry)));
      body.append(grid);
    }

    elements.stripToggle.hidden = !spotlight || entries.length < 2;
    elements.stripToggle.textContent = state.stripHidden ? "Show members" : "Hide members";
    wakeControls();

    if (activeKey) {
      const tile = [...body.querySelectorAll(".grid-tile")].find((node) => node.dataset.key === activeKey);
      tile?.focus({ preventScroll: true });
    }
  }

  function toggleFocus(key) {
    state.focusedKey = state.focusedKey === key ? null : key;
    if (!state.focusedKey) state.stripHidden = false;
    renderVoiceGrid();
  }

  function wakeControls() {
    window.clearTimeout(state.idleTimer);
    elements.voiceGrid.classList.remove("controls-idle");
    if (!state.focusedKey) return;
    state.idleTimer = window.setTimeout(() => {
      if (elements.voiceGrid.contains(document.activeElement) && document.activeElement !== document.body
        && !document.activeElement.classList.contains("grid-tile")) return;
      elements.voiceGrid.classList.add("controls-idle");
    }, CONTROLS_IDLE_MS);
  }

  function exitGridFullscreen() {
    if (document.fullscreenElement === elements.voiceGrid) {
      document.exitFullscreen().catch(() => {});
    }
  }

  function updateFullscreenButton() {
    const active = document.fullscreenElement === elements.voiceGrid;
    elements.gridFullscreen.setAttribute("aria-label", active ? "Exit fullscreen" : "Enter fullscreen");
    elements.gridFullscreen.title = active ? "Exit fullscreen" : "Fullscreen";
    elements.gridFullscreen.querySelector("use").setAttribute("href", active ? "#icon-fullscreen-exit" : "#icon-fullscreen");
  }

  // ── Simulated speaking ───────────────────────────────────────────────

  function applySpeaking() {
    demoApp.querySelectorAll("[data-speaker]").forEach((node) => {
      node.classList.toggle("speaking", state.speaking.has(node.dataset.speaker));
    });
  }

  function tickSpeaking() {
    if (!isVoiceConnected() || document.hidden) return;
    const next = new Set();
    remoteVoiceMembers().forEach((member) => {
      if (!isMemberMuted(member) && Math.random() < 0.4) next.add(member.id);
    });
    if (!state.muted && !state.deafened && Math.random() < 0.3) next.add(SELF_ID);
    state.speaking = next;
    applySpeaking();
  }

  function startSpeaking() {
    stopSpeaking();
    tickSpeaking();
    state.speakTimer = window.setInterval(tickSpeaking, SPEAK_INTERVAL_MS);
  }

  function stopSpeaking() {
    window.clearInterval(state.speakTimer);
    state.speakTimer = null;
    state.speaking = new Set();
  }

  // ── Header, dock, controls ───────────────────────────────────────────

  function renderHeader() {
    const workspace = currentWorkspace();
    elements.workspaceName.textContent = workspace.name;

    if (isGridVisible()) {
      const channel = voiceChannel();
      const count = channel.participants.length + 1;
      elements.channelSymbol.textContent = "⌁";
      elements.channelName.textContent = channel.name;
      elements.channelTopic.textContent = `Voice · ${count} connected · simulated media`;
      elements.address.textContent = `kergit.local/channels/${workspace.slug}/${channel.id}`;
    } else {
      const channel = currentChannel();
      elements.channelSymbol.textContent = "#";
      elements.channelName.textContent = channel.name;
      elements.channelTopic.textContent = channel.topic;
      elements.messageInput.placeholder = `Message #${channel.name}`;
      elements.address.textContent = `kergit.local/channels/${workspace.slug}/${channel.id}`;
    }

    document.querySelectorAll("[data-hub]").forEach((button) => {
      const active = button.dataset.hub === state.hub;
      button.classList.toggle("active", active);
      button.setAttribute("aria-pressed", String(active));
    });
  }

  function renderVoiceDock() {
    if (!state.voice) {
      elements.voiceDock.hidden = true;
      return;
    }
    const connecting = state.voice.phase === "connecting";
    elements.voiceDock.hidden = false;
    elements.voiceDock.classList.toggle("is-connecting", connecting);
    elements.voiceDockStatus.textContent = connecting ? "Connecting…" : "Voice connected";
    elements.voiceDockChannel.textContent = state.voice.name;
    elements.voiceDockDetail.textContent = `${voiceWorkspace().name} · ${connecting ? "issuing token" : "simulated media"}`;
    elements.voiceDockOpen.setAttribute("aria-label", `Open voice grid for ${state.voice.name}`);
  }

  function updateControl(button, active, activeIcon, inactiveIcon, activeLabel, inactiveLabel) {
    button.setAttribute("aria-pressed", String(active));
    button.setAttribute("aria-label", active ? activeLabel : inactiveLabel);
    const use = button.querySelector("use");
    if (use) use.setAttribute("href", active ? activeIcon : inactiveIcon);
  }

  function renderControls() {
    const muted = state.muted || state.deafened;
    updateControl(elements.mute, muted, "#icon-mic-off", "#icon-mic", "Unmute microphone", "Mute microphone");
    updateControl(elements.gridMic, muted, "#icon-mic-off", "#icon-mic", "Unmute microphone", "Mute microphone");
    elements.gridMic.classList.toggle("toolbar-btn-danger", muted);

    updateControl(elements.deafen, state.deafened, "#icon-headphones-off", "#icon-headphones", "Undeafen audio", "Deafen audio");

    updateControl(elements.gridCamera, state.camera, "#icon-camera", "#icon-camera-off", "Turn camera off", "Turn camera on");
    elements.gridCamera.classList.toggle("toolbar-btn-active", state.camera);
    updateControl(elements.camera, state.camera, "#icon-camera", "#icon-camera", "Turn camera off", "Turn camera on");

    updateControl(elements.gridScreen, state.screen, "#icon-screen-off", "#icon-screen", "Stop sharing screen", "Share screen");
    elements.gridScreen.classList.toggle("toolbar-btn-active", state.screen);
    updateControl(elements.screen, state.screen, "#icon-screen", "#icon-screen", "Stop sharing screen", "Share screen");
  }

  function renderStage() {
    const grid = isGridVisible();
    demoApp.classList.toggle("viewing-voice", grid);
    elements.voiceGrid.hidden = !grid;
    if (grid) {
      renderVoiceGrid();
    } else {
      exitGridFullscreen();
      window.clearTimeout(state.idleTimer);
      elements.voiceGrid.classList.remove("controls-idle", "is-spotlight");
      renderMessages();
    }
  }

  function renderAll() {
    renderHeader();
    renderChannels();
    renderStage();
    renderMembers();
    renderVoiceDock();
    renderControls();
  }

  // Voice changes that do not move the main view only need these parts.
  function renderVoiceState() {
    renderChannels();
    renderVoiceGrid();
    renderVoiceDock();
    renderControls();
  }

  // ── Voice actions ────────────────────────────────────────────────────

  function connectVoice(channel) {
    window.clearTimeout(state.connectTimer);
    stopSpeaking();
    exitGridFullscreen();

    const session = { hub: state.hub, channel: channel.id, name: channel.name, phase: "connecting" };
    state.voice = session;
    state.camera = false;
    state.screen = false;
    state.watching.clear();
    state.focusedKey = null;
    state.stripHidden = false;
    state.viewingGrid = true;
    renderAll();

    state.connectTimer = window.setTimeout(() => {
      if (state.voice !== session) return;
      session.phase = "connected";
      startSpeaking();
      renderAll();
      showToast(`Connected to ${channel.name}. Media is simulated — no microphone, camera, or WebRTC.`);
    }, CONNECT_DELAY_MS);
  }

  function joinVoice(channel, anchor) {
    if (isInVoiceChannel(state.hub, channel.id)) {
      setViewingGrid(true);
      renderAll();
      closePanelsOnMobile();
      return;
    }

    if (state.voice) {
      const from = state.voice.name;
      showPopover(
        anchor,
        "Switch voice channel?",
        `You’re connected to ${from}. Joining ${channel.name} disconnects you from ${from} first.`,
        `Join ${channel.name}`,
        () => {
          connectVoice(channel);
          closePanelsOnMobile();
        }
      );
      return;
    }

    connectVoice(channel);
    closePanelsOnMobile();
  }

  function leaveVoice() {
    if (!state.voice) return;
    const channelName = state.voice.name;
    window.clearTimeout(state.connectTimer);
    stopSpeaking();
    exitGridFullscreen();
    state.voice = null;
    setViewingGrid(false);
    state.camera = false;
    state.screen = false;
    state.muted = false;
    state.deafened = false;
    renderAll();
    showToast(`Left ${channelName}. The simulated voice state was cleared.`);
  }

  function openVoiceGrid() {
    if (!state.voice) return;
    if (state.hub !== state.voice.hub) {
      state.hub = state.voice.hub;
      state.search = "";
      elements.search.value = "";
    }
    setViewingGrid(true);
    renderAll();
    closePanelsOnMobile();
    if (state.voice.phase === "connecting") showToast(`Still connecting to ${state.voice.name}…`);
  }

  function toggleMute() {
    if (state.deafened) {
      state.deafened = false;
      state.muted = false;
    } else {
      state.muted = !state.muted;
    }
    if (state.muted) state.speaking.delete(SELF_ID);
    renderVoiceState();
    showToast(state.muted ? "Microphone muted in the simulation." : "Microphone unmuted in the simulation.");
  }

  function toggleDeafen() {
    state.deafened = !state.deafened;
    if (state.deafened) {
      state.muted = true;
      state.speaking.delete(SELF_ID);
    }
    renderVoiceState();
    showToast(state.deafened ? "Audio deafened locally. No real audio exists." : "Audio undeafened in the simulation.");
  }

  function toggleCamera() {
    state.camera = !state.camera;
    renderVoiceState();
    const hint = isGridVisible() ? "" : " Open the voice grid to see your tile.";
    showToast(state.camera
      ? `Simulated camera on — no camera permission was requested.${hint}`
      : "Camera off.");
  }

  function toggleScreen() {
    state.screen = !state.screen;
    renderVoiceState();
    const hint = isGridVisible() ? "" : " Open the voice grid to see it.";
    showToast(state.screen
      ? `Sharing a simulated screen — no screen-capture prompt.${hint}`
      : "Screen share stopped.");
  }

  // ── Messaging ────────────────────────────────────────────────────────

  function formatTime() {
    return new Intl.DateTimeFormat("en", {
      hour: "2-digit",
      minute: "2-digit",
      hour12: false,
    }).format(new Date());
  }

  function sendMessage(text) {
    const self = selfMember();
    currentMessages().push({
      author: self.name,
      initials: self.initials,
      time: formatTime(),
      text,
      badge: "you",
      color: self.color,
      local: true,
    });
    state.search = "";
    elements.search.value = "";
    renderMessages();
    showToast("Message rendered locally. Nothing was sent or stored.");

    window.clearTimeout(state.botTimer);
    elements.typing.hidden = false;
    const sentHub = state.hub;
    const sentChannel = currentChannelId();
    state.botTimer = window.setTimeout(() => {
      elements.typing.hidden = true;
      state.messages[sentHub][sentChannel].push({
        author: "Kergit bot",
        initials: "KG",
        time: formatTime(),
        text: "Local demo confirmed: your message stayed in this browser tab.",
        badge: "demo",
        color: ["#0d9488", "#0891b2"],
        local: true,
      });
      if (state.hub === sentHub && currentChannelId() === sentChannel && !isGridVisible()) renderMessages();
    }, 1100);
  }

  // ── Event wiring ─────────────────────────────────────────────────────

  document.querySelectorAll("[data-hub]").forEach((button) => {
    button.addEventListener("click", () => {
      state.hub = button.dataset.hub;
      state.search = "";
      elements.search.value = "";
      // Matches the real hub rail: switching hubs leaves the voice grid but
      // keeps the voice connection (the dock stays).
      setViewingGrid(false);
      renderAll();
      showToast(state.voice
        ? `Switched to ${currentWorkspace().name}. You’re still in ${state.voice.name} — use the voice dock to return.`
        : `Switched to ${currentWorkspace().name}. Demo state is still local.`);
    });
  });

  elements.messageForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const text = elements.messageInput.value.trim();
    if (!text) {
      elements.messageInput.focus();
      showToast("Type a message first — it will only appear in this local demo.");
      return;
    }
    elements.messageInput.value = "";
    sendMessage(text);
  });

  elements.search.addEventListener("input", () => {
    state.search = elements.search.value;
    renderMessages();
  });

  elements.mute.addEventListener("click", toggleMute);
  elements.gridMic.addEventListener("click", toggleMute);
  elements.deafen.addEventListener("click", toggleDeafen);
  elements.camera.addEventListener("click", toggleCamera);
  elements.gridCamera.addEventListener("click", toggleCamera);
  elements.screen.addEventListener("click", toggleScreen);
  elements.gridScreen.addEventListener("click", toggleScreen);
  elements.leaveVoice.addEventListener("click", leaveVoice);
  elements.gridLeave.addEventListener("click", leaveVoice);
  elements.voiceDockOpen.addEventListener("click", openVoiceGrid);

  elements.stripToggle.addEventListener("click", () => {
    state.stripHidden = !state.stripHidden;
    renderVoiceGrid();
  });

  const canFullscreen = Boolean(document.fullscreenEnabled && elements.voiceGrid.requestFullscreen);
  elements.gridFullscreen.hidden = !canFullscreen;
  elements.gridFullscreen.addEventListener("click", () => {
    if (document.fullscreenElement) {
      document.exitFullscreen().catch(() => {});
    } else {
      elements.voiceGrid.requestFullscreen().catch(() => {});
    }
  });
  document.addEventListener("fullscreenchange", updateFullscreenButton);

  elements.voiceGrid.addEventListener("pointermove", wakeControls);
  elements.voiceGrid.addEventListener("focusin", wakeControls);

  document.querySelector("#demo-add-hub").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      "Create or join a workspace",
      "The production client supports hub creation and invite codes. This page keeps the flow illustrative.",
      "Preview creation",
      () => showToast("Workspace preview created locally — reload or reset to clear it.")
    );
  });

  document.querySelector("#workspace-menu").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      currentWorkspace().name,
      "Invite, channel, member, and workspace settings live here in the real client.",
      "Copy demo invite",
      () => showToast("Demo invite copied conceptually — no clipboard or server was used.")
    );
  });

  document.querySelector("#composer-add").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      "Attachments",
      "The real Kergit client includes an attachment tray and previews. Uploads are disabled on this static page.",
      "Got it",
      () => showToast("No file picker opened and no data left this page.")
    );
  });

  document.querySelectorAll(".optional-action").forEach((button) => {
    button.addEventListener("click", () => {
      showToast("This control is present for product fidelity; its data is not loaded in the static demo.");
    });
  });

  elements.channelsToggle.addEventListener("click", () => {
    const opening = !demoApp.classList.contains("channels-open");
    closePanels();
    demoApp.classList.toggle("channels-open", opening);
    elements.channelsToggle.setAttribute("aria-expanded", String(opening));
  });

  elements.membersToggle.addEventListener("click", () => {
    if (window.matchMedia(COMPACT_QUERY).matches) {
      const opening = !demoApp.classList.contains("members-open");
      closePanels();
      demoApp.classList.toggle("members-open", opening);
      elements.membersToggle.setAttribute("aria-expanded", String(opening));
      return;
    }

    const hidden = demoApp.classList.toggle("members-hidden");
    elements.membersToggle.setAttribute("aria-expanded", String(!hidden));
  });

  elements.scrim.addEventListener("click", closePanels);

  elements.reset.addEventListener("click", () => {
    window.clearTimeout(state.botTimer);
    window.clearTimeout(state.connectTimer);
    window.clearTimeout(state.idleTimer);
    stopSpeaking();
    exitGridFullscreen();
    elements.typing.hidden = true;
    state.hub = "kergit";
    state.channelByHub = { ...initialChannelByHub };
    state.messages = JSON.parse(JSON.stringify(initialMessages));
    state.voice = null;
    setViewingGrid(false);
    state.muted = false;
    state.deafened = false;
    state.camera = false;
    state.screen = false;
    state.search = "";
    elements.search.value = "";
    elements.messageInput.value = "";
    demoApp.classList.remove("members-hidden");
    closePanels();
    renderAll();
    showToast("Demo reset to its original local state.");
  });

  window.addEventListener("resize", () => {
    closePanels();
    if (!window.matchMedia(COMPACT_QUERY).matches) {
      elements.membersToggle.setAttribute("aria-expanded", String(!demoApp.classList.contains("members-hidden")));
    }
  });

  document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape") return;
    const popoverOpen = !elements.popover.hidden;
    const panelOpen = demoApp.classList.contains("channels-open") || demoApp.classList.contains("members-open");
    if (!popoverOpen && !panelOpen && state.focusedKey && isGridVisible()
      && demoApp.contains(document.activeElement)) {
      toggleFocus(state.focusedKey);
      return;
    }
    closePanels();
  });

  renderAll();
})();
