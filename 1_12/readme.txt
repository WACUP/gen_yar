     Yar-matey! Playlist Copier v1.12 (23/10/2013)


v1.11 of this plugin has been built against the 5.66 SDK.
For the plugin to be built correctly it assumes the source
and SDK files are setup in the following locations:

<gen_yar>
<sdk>\<winamp>

(This is unlike builds prior to 1.10 where older SDK files
 were contained within the source bundle).


About this update

This update of the plugin is restricted to running on Winamp
clients 5.64 and up due to api dependancies which allow it
to regsiter the Alt+C (and alternative) shortcut correctly
as well as additionally unicode support (hence 5.64+ only).

Also it has been turned into a complete unicode based plugin
to allow it to correctly handle the transfer of unicode files
and also now allows for creation of a m3u8 (unicode) playlist.

[Amendment 20/01/2010]

Source and build tools are now included in the installer package.
When compiled, gen_yar will be generated in the Release folder
and the language file will be generated in Release\LangFiles.

Source and language files are installed into the following:

%installdir%\Plugins\gen_yar\readme.txt
%installdir%\Plugins\gen_yar\source
%installdir%\Plugins\gen_yar\Language File\gen_yar.lng
%installdir%\Plugins\gen_yar\installer script\gen_yar.nsi