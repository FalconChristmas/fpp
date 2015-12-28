<!DOCTYPE html>
<html>
<head>
    <title>Falcon Player</title>
    <meta charset="utf-8">
    <meta name="csrf-token" content="{{ csrf_token() }}">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="apple-touch-icon" href="/apple-touch-icon-precomposed.png">

    <link href="https://fonts.googleapis.com/css?family=Roboto:400,300,100&subset=latin,latin-ext,vietnamese,greek-ext,greek,cyrillic,cyrillic-ext" rel="stylesheet">
    
    @yield('styles')

    <link rel="stylesheet" href="{{ elixir('css/app.css') }}">

    @yield('scripts_head')
</head>
<body>

    <app></app>

   
    <script src="{{ elixir('js/main.js') }}"></script>
    @yield('scripts_foot')
</body>
</html>
